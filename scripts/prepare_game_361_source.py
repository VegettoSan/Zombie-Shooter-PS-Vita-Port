#!/usr/bin/env python3
"""Prepare the verified Zombie Shooter 3.6.1 ARMv7 compatibility profile.

The repository keeps the old hardware-tested implementation readable, but the
3.6.1 binary moved its native functions.  This preparer rebases only hooks that
were re-verified against build 1161 by symbol + Thumb entry + prologue.  It also
applies the 3.6.1 gameplay action mapping and makes SharedPreferences mutations
durable immediately so hardware tests can distinguish a JNI routing failure
from a missing commit/apply call.

Every replacement is deliberately exact.  Source drift aborts the build rather
than applying a patch to an unknown function/layout.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def patch_file(relative, replacements):
    path = ROOT / relative
    text = path.read_text(encoding="utf-8")
    changed = False
    for old, new, label in replacements:
        if new in text:
            continue
        count = text.count(old)
        if count != 1:
            raise SystemExit(
                f"{relative}: expected exactly one {label} source pattern, found {count}; "
                "refusing to patch blindly"
            )
        text = text.replace(old, new, 1)
        changed = True
    if changed:
        path.write_text(text, encoding="utf-8")
    return changed


# Java-facing package/version identity used by the native engine.
patch_file("source/java_base.inc", [
    ('return jni->NewStringUTF(&jni, "3.5.3");',
     'return jni->NewStringUTF(&jni, "3.6.1");', "getVersion"),
    ('return 1153;', 'return 1161;', "getVersionCode"),
    ("const int SDK_INT = 24; // Matches the APK's minSdk and native build target.",
     "const int SDK_INT = 24; // Matches the ARMv7 native ELF/API target used by the compatibility layer.",
     "SDK comment"),
])

# Hardware log #38 proved that RegistryPrivate reaches SharedPreferences but
# every value is removed and AES-256 rejects its key.  Build-1161 disassembly
# closes the native chain:
#
#   CryptEngine::encryptionKey()
#     -> salt()
#     -> applicationUID()
#     -> Settings.Secure.getString(ContentResolver, "android_id")
#     -> sha256(android_id)
#     -> 32-byte AES-256 key
#
# SigmaTeam's sha256 helper intentionally returns an EMPTY vector when the
# Java string is null/empty. Chipher<1,256>::porcess then compares key.size()
# with 0x20 and emits exactly the "Wrong AES key length" seen on hardware.
# Resolve this Java path by exact class+signature, not FalsoJNI's legacy
# name-only fallback. The Android ID itself remains the normal stable 16 hex
# characters; the engine hashes it to the required 32 bytes.
old_secure = '''static jobject secureGetString(jmethodID id, va_list args) {
\t(void) id;
\tjobject resolver = va_arg(args, jobject);
\tjstring name = va_arg(args, jstring);
\tif (resolver != (jobject)0x71717171 || !name)
\t\treturn NULL;
\tconst char *key = jni->GetStringUTFChars(&jni, name, NULL);
\tif (!key)
\t\treturn NULL;
\tint is_android_id = strcmp(key, "android_id") == 0;
\tjni->ReleaseStringUTFChars(&jni, name, (char *)key);
\tif (!is_android_id)
\t\treturn NULL;
\t/* Settings.Secure.ANDROID_ID is a stable 64-bit hexadecimal identifier.
\t * Keep it stable across runs so encrypted local saves keep the same key. */
\treturn jni->NewStringUTF(&jni, "a1b2c3d4e5f60718");
}'''
new_secure = '''static jobject secureGetString(jmethodID id, va_list args) {
\t(void) id;
\tjobject resolver = va_arg(args, jobject);
\tjstring name = va_arg(args, jstring);
\tif (!name)
\t\treturn NULL;
\tconst char *key = jni->GetStringUTFChars(&jni, name, NULL);
\tif (!key)
\t\treturn NULL;
\tint is_android_id = strcmp(key, "android_id") == 0;
#ifdef ZOMBIE_DEBUG_BUILD
\tstatic unsigned crypto_identity_reports;
\tif (__atomic_fetch_add(&crypto_identity_reports, 1, __ATOMIC_RELAXED) < 8)
\t\tl_perf("[CRYPTO] Settings.Secure.getString name=%s resolver=%p resolver_expected=%d android_id=%d",
\t\t       key, resolver, resolver == (jobject)0x71717171, is_android_id);
#endif
\tjni->ReleaseStringUTFChars(&jni, name, (char *)key);
\tif (!is_android_id)
\t\treturn NULL;
\t/* applicationUID() SHA-256 hashes this normal 64-bit Android identifier.
\t * Do not return a pre-hashed/32-character value here: that would change the
\t * engine's real key derivation contract. Resolver identity is intentionally
\t * not used as a rejection condition once this exact static API is resolved;
\t * Android accepts any valid ContentResolver and the Vita object is synthetic. */
\treturn jni->NewStringUTF(&jni, "a1b2c3d4e5f60718");
}'''

old_resolver = '''int fjni_resolve_method(jclass clazz, const char *name, const char *sig, jboolean is_static, jmethodID *result) {
    if (clazz == (jclass)0x42424242 && !strcmp(name, "getWindowManager") &&
        !strcmp(sig, "()Landroid/view/WindowManager;") && !is_static) {
        *result = (jmethodID)(uintptr_t)METHOD_GET_WINDOW_MANAGER; return 1;
    }
'''
new_resolver = '''int fjni_resolve_method(jclass clazz, const char *name, const char *sig, jboolean is_static, jmethodID *result) {
    /* CryptEngine::applicationUID() asks the main Activity for a resolver and
     * then calls Settings.Secure.getString(ContentResolver,String). Resolve
     * both calls by exact descriptor so the registry encryption key never
     * depends on FalsoJNI's name-only fallback. Consume every same-name query
     * on these classes: a wrong descriptor must be NULL, never fall through to
     * legacy name-only lookup. */
    if (clazz == (jclass)0x42424242 && !strcmp(name, "getContentResolver")) {
        *result = NULL;
        if (!is_static && !strcmp(sig, "()Landroid/content/ContentResolver;"))
            *result = (jmethodID)(uintptr_t)METHOD_GET_CONTENT_RESOLVER;
        return 1;
    }
    if (class_matches(clazz, "android/provider/Settings$Secure") && is_static) {
        *result = NULL;
        if (!strcmp(name, "getString") &&
            !strcmp(sig, "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;"))
            *result = (jmethodID)(uintptr_t)METHOD_SECURE_GET_STRING;
        return 1;
    }
    if (clazz == (jclass)0x42424242 && !strcmp(name, "getWindowManager") &&
        !strcmp(sig, "()Landroid/view/WindowManager;") && !is_static) {
        *result = (jmethodID)(uintptr_t)METHOD_GET_WINDOW_MANAGER; return 1;
    }
'''
patch_file("source/java_base.inc", [
    (old_secure, new_secure, "Settings.Secure android_id implementation"),
    (old_resolver, new_resolver, "exact crypto identity JNI resolver"),
])

# The regression follows the same JNI calls applicationUID() makes. It proves
# exact descriptor matching, a non-null ContentResolver path, a stable 16-char
# Android ID and rejection of unrelated Settings.Secure keys. This catches the
# failure before a VPK can be produced.
crypto_regression_anchor = ''' puts("DisplayMetrics JNI regression passed: exact descriptors, nonzero fields and complete activity/display chain");

 jclass clazz=jni->FindClass(&jni,"android/view/InputDevice");'''
crypto_regression = ''' puts("DisplayMetrics JNI regression passed: exact descriptors, nonzero fields and complete activity/display chain");

 /* Build-1161 CryptEngine::applicationUID() uses this exact Android chain,
  * then SHA-256 hashes the returned 16 ASCII hex characters into 32 bytes. */
 jobject crypto_activity=(jobject)0x42424242;
 jmethodID content_resolver_id=jni->GetMethodID(&jni,(jclass)crypto_activity,"getContentResolver","()Landroid/content/ContentResolver;");
 assert(content_resolver_id);
 assert(!jni->GetMethodID(&jni,(jclass)crypto_activity,"getContentResolver","()Ljava/lang/Object;"));
 jobject content_resolver=jni->CallObjectMethod(&jni,crypto_activity,content_resolver_id); assert(content_resolver);
 jclass secure=jni->FindClass(&jni,"android.provider.Settings$Secure"); assert(secure);
 jmethodID secure_get=jni->GetStaticMethodID(&jni,secure,"getString","(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;");
 assert(secure_get);
 assert(!jni->GetStaticMethodID(&jni,secure,"getString","(Ljava/lang/String;)Ljava/lang/String;"));
 jstring android_id_name=jni->NewStringUTF(&jni,"android_id");
 jstring android_id_value=jni->CallStaticObjectMethod(&jni,secure,secure_get,content_resolver,android_id_name); assert(android_id_value);
 const char *android_id_utf=jni->GetStringUTFChars(&jni,android_id_value,NULL); assert(android_id_utf);
 assert(strlen(android_id_utf)==16 && !strcmp(android_id_utf,"a1b2c3d4e5f60718"));
 jni->ReleaseStringUTFChars(&jni,android_id_value,(char*)android_id_utf);
 jstring unknown_secure_name=jni->NewStringUTF(&jni,"unknown_secure_key");
 assert(!jni->CallStaticObjectMethod(&jni,secure,secure_get,content_resolver,unknown_secure_name));
 jni->DeleteGlobalRef(&jni,android_id_name); jni->DeleteGlobalRef(&jni,android_id_value);
 jni->DeleteGlobalRef(&jni,unknown_secure_name); jni->DeleteGlobalRef(&jni,secure);
 puts("Crypto identity JNI regression passed: exact Settings.Secure android_id path and stable non-empty UID");

 jclass clazz=jni->FindClass(&jni,"android/view/InputDevice");'''
patch_file("tests/input_device_regression.c", [
    (crypto_regression_anchor, crypto_regression, "CryptEngine applicationUID JNI regression"),
])

# MusicPlayer layout was checked in 3.6.1: BaseStream volume remains self+0x20,
# loop remains self+0x28 and MusicPlayer's should-stop flag remains self+0x44.
# The six virtual methods and helper functions keep the same verified prologues.
patch_file("source/utils/audio_stream.c", [
    ('{"_ZN5sound11MusicPlayer6onOpenERK6STRING",0x4d4174,',
     '{"_ZN5sound11MusicPlayer6onOpenERK6STRING",0x4ef2f0,', "MusicPlayer::onOpen offset"),
    ('{"_ZN5sound11MusicPlayer7onCloseEv",0x4d46e8,',
     '{"_ZN5sound11MusicPlayer7onCloseEv",0x4ef864,', "MusicPlayer::onClose offset"),
    ('{"_ZN5sound11MusicPlayer8onUpdateEv",0x4d485c,',
     '{"_ZN5sound11MusicPlayer8onUpdateEv",0x4ef9d8,', "MusicPlayer::onUpdate offset"),
    ('{"_ZN5sound11MusicPlayer7onPauseEv",0x4d4990,',
     '{"_ZN5sound11MusicPlayer7onPauseEv",0x4efb0c,', "MusicPlayer::onPause offset"),
    ('{"_ZN5sound11MusicPlayer8onResumeEv",0x4d4ad0,',
     '{"_ZN5sound11MusicPlayer8onResumeEv",0x4efc4c,', "MusicPlayer::onResume offset"),
    ('{"_ZN5sound11MusicPlayer12updateVolumeEi",0x4d4058,',
     '{"_ZN5sound11MusicPlayer12updateVolumeEi",0x4ef1d4,', "MusicPlayer::updateVolume offset"),
    ('so_mod.load_addr+0x3dc789u', 'so_mod.load_addr+0x3ed769u', "STRING::c_str Thumb address"),
    ('so_mod.load_addr+0x4d4da9u', 'so_mod.load_addr+0x4eff25u', "BaseStream::stop Thumb address"),
])

# Restore the physically useful 864-wide software surface, but only at the
# exact 3.6.1 functions with the already guarded prologue trampoline.
patch_file("source/utils/render_scale.c", [
    ('entry != so_mod.load_addr + 0x481db0',
     'entry != so_mod.load_addr + 0x4959c0', "scale_config::calculateFactor offset"),
    ('getter != so_mod.load_addr + 0x4820d1',
     'getter != so_mod.load_addr + 0x495de1', "scale_config::factor Thumb address"),
    ('setter != so_mod.load_addr + 0x4820dd',
     'setter != so_mod.load_addr + 0x495ded', "scale_config::setFactor Thumb address"),
])

# Rebase only the low-frequency engine probes/frame-reuse functions whose
# mangled symbols, signatures and first 8 bytes were verified in build 1161.
# The old raster/light/palette hooks are deliberately NOT enabled yet.
patch_file("source/patch.c", [
    ('{"_ZN5GRAPH4TactEi",0x410a30,',
     '{"_ZN5GRAPH4TactEi",0x421d70,', "GRAPH::Tact offset"),
    ('{"_ZN5GRAPH12softwareTactEi",0x410458,',
     '{"_ZN5GRAPH12softwareTactEi",0x421798,', "GRAPH::softwareTact offset"),
    ('{"_ZN3MAP4tactEv",0x4372c4,',
     '{"_ZN3MAP4tactEv",0x448b00,', "MAP::tact offset"),
    ('{"_ZN8OpenGLES7preTactEv",0x46aa2c,',
     '{"_ZN8OpenGLES7preTactEv",0x47d9a0,', "OpenGLES::preTact offset"),
    ('{"_ZN8OpenGLES8PostTactEi",0x46af1c,',
     '{"_ZN8OpenGLES8PostTactEi",0x47de90,', "OpenGLES::PostTact offset"),
    ('{"_ZNK16SPRITE_COLLECTOR9DrawLayerEiRK7VECTOR2S2_bb",0x4f4b20,',
     '{"_ZNK16SPRITE_COLLECTOR9DrawLayerEiRK7VECTOR2S2_bb",0x5108d0,', "SPRITE_COLLECTOR::DrawLayer offset"),
    ('\tinstall_startup_diagnostics();',
     '\t/* 3.6.1: startup diagnostics disabled until independently revalidated. */',
     "disable legacy startup hooks"),
    ('    install_contract_traces();',
     '    /* 3.6.1: legacy contract traces disabled. */', "disable legacy contract traces"),
    ('\traster_palette_install();',
     '\t/* 3.6.1: old palette hook disabled. */', "disable legacy palette hook"),
    ('\traster_alpha_install();',
     '\t/* 3.6.1: old alpha hook disabled. */', "disable legacy alpha hook"),
    ('\traster_light_install();',
     '\t/* 3.6.1: old raster-light hook disabled. */', "disable legacy raster-light hook"),
    ('\tlight_pipeline_install();',
     '\t/* 3.6.1: old light-pipeline hook disabled. */', "disable legacy light-pipeline hook"),
])

# The bring-up build used kuser_patch() directly, which intentionally skipped
# music, scaling and adaptive software-frame reuse. Switch to the verified
# 3.6.1 subset above; so_patch() still performs kuser first.
patch_file("source/utils/init.c", [
    ('    kuser_patch();', '    so_patch();', "3.6.1 recovery patch entry"),
    ('Applying 3.6.1 safe bring-up patches (kuser/protobuf only)...',
     'Applying verified 3.6.1 recovery patches (kuser + frame reuse + music + render scale)...',
     "recovery start log"),
    ('3.6.1 safe bring-up patches applied; legacy game-specific hooks disabled.',
     '3.6.1 recovery patches applied; unsafe legacy raster/light/startup hooks remain disabled.',
     "recovery completion log"),
])

# Gameplay mapping recovered from the 3.6.1 joystick menu resources:
#   DPAD_LEFT/action7  = previous weapon
#   DPAD_RIGHT/action8 = next weapon
#   DPAD_UP/LTRIGGER/action9 = medikit
#   DPAD_DOWN/RTRIGGER/action10 = grenade
# Keep rear touch as LT/RT and use the known HAT actions for the Vita L/R pair.
# This avoids the old LB/RB path that produced mixed/wrong actions on hardware.
gamepad_migration = '''    /* Auto-migrate only the untouched old stock profile. Custom controls.txt
     * files keep their explicit choices. */
    if (bindings[0].xbox_mask == SCE_CTRL_CROSS &&
        bindings[1].xbox_mask == SCE_CTRL_CIRCLE &&
        bindings[2].xbox_mask == SCE_CTRL_SQUARE &&
        bindings[3].xbox_mask == SCE_CTRL_TRIANGLE &&
        bindings[4].xbox_mask == SCE_CTRL_L1 &&
        bindings[5].xbox_mask == SCE_CTRL_R1 &&
        bindings[6].xbox_mask == SCE_CTRL_L2 &&
        bindings[7].xbox_mask == SCE_CTRL_R2 &&
        bindings[10].xbox_mask == SCE_CTRL_UP &&
        bindings[11].xbox_mask == SCE_CTRL_DOWN &&
        bindings[12].xbox_mask == SCE_CTRL_LEFT &&
        bindings[13].xbox_mask == SCE_CTRL_RIGHT) {
        bindings[4].xbox_mask = SCE_CTRL_RIGHT; /* L -> next weapon/action8 */
        bindings[5].xbox_mask = SCE_CTRL_LEFT;  /* R -> previous weapon/action7 */
        l_perf("[INPUT] controls auto_migrated=361_gameplay l=DPAD_RIGHT r=DPAD_LEFT rear_left=LT rear_right=RT");
    }

'''
patch_file("source/utils/gamepad.c", [
    ('bindings[4].xbox_mask  = SCE_CTRL_L1;       /* LB */',
     'bindings[4].xbox_mask  = SCE_CTRL_RIGHT;    /* next weapon / action8 */', "default Vita L action"),
    ('bindings[5].xbox_mask  = SCE_CTRL_R1;       /* RB */',
     'bindings[5].xbox_mask  = SCE_CTRL_LEFT;     /* previous weapon / action7 */', "default Vita R action"),
    ('          "l LB\\n"\n          "r RB\\n"',
     '          "# 3.6.1 gameplay aliases: shoulders use the proven weapon HAT actions.\\n"\n'
     '          "l DPAD_RIGHT\\n"\n          "r DPAD_LEFT\\n"', "generated controls.txt shoulders"),
    ('    fclose(f);\n\n    controls_loaded = true;\n    log_effective_mapping("controls.txt");',
     '    fclose(f);\n\n' + gamepad_migration + '    controls_loaded = true;\n    log_effective_mapping("controls.txt");',
     "stock controls.txt auto-migration"),
])

# Do not rely on the engine eventually calling Editor.commit()/apply(). A real
# Android SharedPreferences editor normally commits staged mutations there, but
# our lightweight bridge mutates the in-memory map immediately. Persist that
# mutation immediately as well so progress cannot vanish on abrupt Vita exits.
patch_file("source/preferences_jni.inc", [
    ('if(v)ok=pref_set(k,v);else pref_remove_key(k);SAVE_TRACE(',
     'if(v)ok=pref_set(k,v);else pref_remove_key(k);if(ok&&pref_dirty)ok=pref_save();SAVE_TRACE(',
     "durable SharedPreferences putString"),
    ('pref_remove_key(k);pref_unlock();jni->ReleaseStringUTFChars',
     'pref_remove_key(k);int ok=pref_save();SAVE_TRACE("[SAVE] durable remove hash=%08X ok=%d",pref_key_hash(k),ok);if(!ok)l_error("[SAVE] durable remove failed");pref_unlock();jni->ReleaseStringUTFChars',
     "durable SharedPreferences remove"),
    ('pref_free_all();pref_dirty=1;pref_unlock();return(jobject)&prefs_editor_object;',
     'pref_free_all();pref_dirty=1;int ok=pref_save();SAVE_TRACE("[SAVE] durable clear ok=%d",ok);if(!ok)l_error("[SAVE] durable clear failed");pref_unlock();return(jobject)&prefs_editor_object;',
     "durable SharedPreferences clear"),
])

print("Prepared Zombie Shooter 3.6.1 build 1161 profile:")
print("  - Java identity / native API 24")
print("  - exact Settings.Secure android_id crypto identity path")
print("  - verified M4A/AAC music hooks rebased")
print("  - verified 864 render-scale hook rebased")
print("  - verified engine probes + adaptive software-frame reuse rebased")
print("  - unsafe legacy raster/light/startup hooks kept disabled")
print("  - Vita L/R mapped to proven next/previous weapon actions")
print("  - SharedPreferences mutations persisted immediately")
