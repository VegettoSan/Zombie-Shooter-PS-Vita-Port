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
# music, scaling and adaptive software-frame reuse.  Switch to the verified
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

# Do not rely on the engine eventually calling Editor.commit()/apply().  A real
# Android SharedPreferences editor normally commits staged mutations there, but
# our lightweight bridge mutates the in-memory map immediately.  Persist that
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
print("  - verified M4A/AAC music hooks rebased")
print("  - verified 864 render-scale hook rebased")
print("  - verified engine probes + adaptive software-frame reuse rebased")
print("  - unsafe legacy raster/light/startup hooks kept disabled")
print("  - Vita L/R mapped to proven next/previous weapon actions")
print("  - SharedPreferences mutations persisted immediately")
