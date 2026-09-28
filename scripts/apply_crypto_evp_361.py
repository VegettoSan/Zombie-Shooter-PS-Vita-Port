#!/usr/bin/env python3
"""Inject the build-1161 direct Registry file backend compatibility hook.

Zombie Shooter 3.6.1 routes persistent game state through core::Registry.
On Android, encrypted Registry values are transformed by storeEncrypted() and
then written as SharedPreferences strings. On Vita, the bundled OpenSSL 3
provider cannot execute that AES-256-CBC path (real-hardware error 0308010C),
so no value ever reaches SharedPreferences.

Match the pattern used by MetalSyntax ports: keep the game's logical save API,
but replace the platform persistence boundary with direct files. The hook
intercepts only Registry::storeEncrypted/loadDecrypted for exact build 1161.
It still calls SigmaTeam's own Registry::encryptKey(), so contains/remove and
RegistryEnumerator address the same backend keys. Values are stored unchanged
by the Vita backend under ux0:data/zombieshooter/save/*.dat.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PATCH = ROOT / "source/patch.c"
MARKER = "ZS361_DIRECT_REGISTRY_SAVE"

HOOK_CODE = r'''
#include <stdlib.h>
/* ZS361_DIRECT_REGISTRY_SAVE
 *
 * Build 1161's Registry::setString/getString route persistent encrypted values
 * through storeEncrypted/loadDecrypted.  OpenSSL AES-256-CBC is unavailable in
 * the Vita runtime provider, so bypass only that crypto boundary and persist
 * the original STRING value through the direct Vita Registry backend.
 * Registry::encryptKey remains the game's own implementation, preserving the
 * exact key namespace expected by contains/remove/enumeration.
 */
extern int zombie_registry_direct_put(const char *key, const char *value);
extern char *zombie_registry_direct_get(const char *key);

static so_hook zs361_registry_store_encrypted_hook;
static so_hook zs361_registry_load_decrypted_hook;

typedef const char *(*zs361_string_c_str_fn)(const void *self);
typedef void (*zs361_string_ctor_cstr_fn)(void *self, const char *value);
typedef void (*zs361_string_copy_ctor_fn)(void *self, const void *other);
typedef void (*zs361_string_dtor_fn)(void *self);
typedef void (*zs361_encrypt_key_fn)(void *result, void *registry, const void *key);

static zs361_string_c_str_fn zs361_string_c_str;
static zs361_string_ctor_cstr_fn zs361_string_ctor_cstr;
static zs361_string_copy_ctor_fn zs361_string_copy_ctor;
static zs361_string_dtor_fn zs361_string_dtor;
static zs361_encrypt_key_fn zs361_encrypt_key;

typedef struct {
    uintptr_t words[3]; /* build-1161 STRING is exactly 12 bytes */
} zs361_string_storage;

#define ZS361_REG_STORE_ENCRYPTED_ENTRY 0x003FB984u
#define ZS361_REG_LOAD_DECRYPTED_ENTRY  0x003FB054u
#define ZS361_REG_ENCRYPT_KEY_ENTRY     0x003FBFF4u
#define ZS361_STRING_C_STR_ENTRY        0x003ED768u
#define ZS361_STRING_CTOR_CSTR_ENTRY    0x003ED7B0u
#define ZS361_STRING_COPY_CTOR_ENTRY    0x003ED800u
#define ZS361_STRING_DTOR_ENTRY         0x003EE644u

static uintptr_t zs361_even(uintptr_t address) {
    return address & ~(uintptr_t)1u;
}

static uintptr_t zs361_require_thumb_symbol(const char *name, uintptr_t expected_offset) {
    uintptr_t symbol = so_symbol(&so_mod, name);
    uintptr_t expected = so_mod.load_addr + expected_offset;
    if (!symbol || !(symbol & 1u) || zs361_even(symbol) != expected) {
        l_error("[SAVE] refusing direct Registry hook %s: symbol=%p expected=so+0x%08X Thumb",
                name, (void *)symbol, (unsigned)expected_offset);
        return 0;
    }
    return symbol;
}

static int zs361_validate_prologue(uintptr_t symbol, const uint8_t expected[8],
                                   const char *name) {
    uintptr_t entry = zs361_even(symbol);
    if (memcmp((const void *)entry, expected, 8) != 0) {
        l_error("[SAVE] refusing direct Registry hook %s: build-1161 prologue mismatch", name);
        return 0;
    }
    return 1;
}

static int zs361_make_backend_key(zs361_string_storage *out, void *registry,
                                  const void *key, const char **chars_out) {
    memset(out, 0, sizeof(*out));
    zs361_encrypt_key(out, registry, key);
    const char *chars = zs361_string_c_str(out);
    if (!chars || !*chars) {
        zs361_string_dtor(out);
        return 0;
    }
    *chars_out = chars;
    return 1;
}

static void zs361_hooked_registry_store_encrypted(void *registry,
                                                   const void *key,
                                                   const void *value) {
    zs361_string_storage backend_key;
    const char *backend_chars = NULL;
    const char *value_chars = zs361_string_c_str(value);
    if (!value_chars || !zs361_make_backend_key(&backend_key, registry, key, &backend_chars)) {
        l_error("[SAVE] direct Registry store rejected invalid STRING/key");
        return;
    }

    int ok = zombie_registry_direct_put(backend_chars, value_chars);
#ifdef ZOMBIE_DEBUG_BUILD
    static unsigned reports;
    if (__atomic_fetch_add(&reports, 1, __ATOMIC_RELAXED) < 128)
        l_perf("[SAVE] Registry.storeEncrypted -> direct file bytes=%u ok=%d",
               (unsigned)strlen(value_chars), ok);
#endif
    zs361_string_dtor(&backend_key);
}

static void zs361_hooked_registry_load_decrypted(void *result,
                                                  void *registry,
                                                  const void *key,
                                                  const void *fallback) {
    zs361_string_storage backend_key;
    const char *backend_chars = NULL;
    char *value = NULL;
    int key_ok = zs361_make_backend_key(&backend_key, registry, key, &backend_chars);
    if (key_ok)
        value = zombie_registry_direct_get(backend_chars);

    if (value)
        zs361_string_ctor_cstr(result, value);
    else
        zs361_string_copy_ctor(result, fallback);

#ifdef ZOMBIE_DEBUG_BUILD
    static unsigned reports;
    if (__atomic_fetch_add(&reports, 1, __ATOMIC_RELAXED) < 128)
        l_perf("[SAVE] Registry.loadDecrypted <- direct file hit=%d bytes=%u",
               value != NULL, value ? (unsigned)strlen(value) : 0u);
#endif
    free(value);
    if (key_ok)
        zs361_string_dtor(&backend_key);
}

static void install_zs361_direct_registry_save(void) {
    static const uint8_t store_prologue[8] = {0xF0,0xB5,0x03,0xAF,0x2D,0xE9,0x00,0x0B};
    static const uint8_t load_prologue[8]  = {0xF0,0xB5,0x03,0xAF,0x4D,0xF8,0x04,0x8D};

    uintptr_t store = zs361_require_thumb_symbol(
        "_ZN4core8Registry14storeEncryptedERK6STRINGS3_", ZS361_REG_STORE_ENCRYPTED_ENTRY);
    uintptr_t load = zs361_require_thumb_symbol(
        "_ZN4core8Registry13loadDecryptedERK6STRINGS3_", ZS361_REG_LOAD_DECRYPTED_ENTRY);
    uintptr_t encrypt_key = zs361_require_thumb_symbol(
        "_ZN4core8Registry10encryptKeyERK6STRING", ZS361_REG_ENCRYPT_KEY_ENTRY);
    uintptr_t c_str = zs361_require_thumb_symbol(
        "_ZNK6STRING5c_strEv", ZS361_STRING_C_STR_ENTRY);
    uintptr_t ctor_cstr = zs361_require_thumb_symbol(
        "_ZN6STRINGC1EPKc", ZS361_STRING_CTOR_CSTR_ENTRY);
    uintptr_t copy_ctor = zs361_require_thumb_symbol(
        "_ZN6STRINGC1ERKS_", ZS361_STRING_COPY_CTOR_ENTRY);
    uintptr_t dtor = zs361_require_thumb_symbol(
        "_ZN6STRINGD1Ev", ZS361_STRING_DTOR_ENTRY);

    if (!store || !load || !encrypt_key || !c_str || !ctor_cstr || !copy_ctor || !dtor ||
        !zs361_validate_prologue(store, store_prologue, "Registry::storeEncrypted") ||
        !zs361_validate_prologue(load, load_prologue, "Registry::loadDecrypted")) {
        l_error("[SAVE] direct Registry file backend disabled; exact build-1161 guards failed");
        return;
    }

    zs361_encrypt_key = (zs361_encrypt_key_fn)encrypt_key;
    zs361_string_c_str = (zs361_string_c_str_fn)c_str;
    zs361_string_ctor_cstr = (zs361_string_ctor_cstr_fn)ctor_cstr;
    zs361_string_copy_ctor = (zs361_string_copy_ctor_fn)copy_ctor;
    zs361_string_dtor = (zs361_string_dtor_fn)dtor;

    zs361_registry_store_encrypted_hook = hook_addr(
        store, (uintptr_t)&zs361_hooked_registry_store_encrypted);
    zs361_registry_load_decrypted_hook = hook_addr(
        load, (uintptr_t)&zs361_hooked_registry_load_decrypted);
    l_info("[SAVE] installed 3.6.1 direct Registry backend -> ux0:data/zombieshooter/save/");
}
'''


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"patch.c: expected one {label} anchor, found {count}; refusing drift")
    return text.replace(old, new, 1)


def remove_legacy_evp_patch(text: str) -> str:
    legacy = "/* ZS361_EVP_METADATA_COMPAT"
    if legacy not in text:
        return text
    end_marker = '    l_info("[CRYPTO] installed guarded 3.6.1 AES-256 EVP metadata compatibility (Thumb-safe)");\n}\n'
    start = text.index(legacy)
    end = text.find(end_marker, start)
    if end < 0:
        raise SystemExit("patch.c: legacy EVP marker found but block end is unknown; refusing drift")
    end += len(end_marker)
    text = text[:start] + text[end:]
    call = "\tinstall_zs361_crypto_evp_compat();\n"
    if text.count(call) != 1:
        raise SystemExit("patch.c: legacy EVP block found but installer call missing/duplicated")
    text = text.replace(call, "", 1)
    print("Removed obsolete EVP metadata hook from prepared patch.c")
    return text


def main() -> None:
    text = PATCH.read_text(encoding="utf-8")
    text = remove_legacy_evp_patch(text)
    if MARKER in text:
        if text.count("install_zs361_direct_registry_save();") != 1:
            raise SystemExit("patch.c: direct Registry marker exists but installer call is missing/duplicated")
        PATCH.write_text(text, encoding="utf-8")
        print("3.6.1 direct Registry file backend already prepared")
        return

    text = replace_once(
        text,
        "static so_hook registry_load_value_hook;\n",
        "static so_hook registry_load_value_hook;\n\n" + HOOK_CODE + "\n",
        "registry hook declaration",
    )
    text = replace_once(
        text,
        "\tkuser_patch();\n",
        "\tkuser_patch();\n\tinstall_zs361_direct_registry_save();\n",
        "kuser_patch installer",
    )

    required = [
        "ZS361_DIRECT_REGISTRY_SAVE",
        "0x003FB984u", "0x003FB054u", "0x003FBFF4u",
        "_ZN4core8Registry14storeEncryptedERK6STRINGS3_",
        "_ZN4core8Registry13loadDecryptedERK6STRINGS3_",
        "_ZN4core8Registry10encryptKeyERK6STRING",
        "zombie_registry_direct_put", "zombie_registry_direct_get",
        "hook_addr(\n        store", "hook_addr(\n        load",
        "install_zs361_direct_registry_save();",
    ]
    missing = [token for token in required if token not in text]
    if missing:
        raise SystemExit("patch.c: generated direct Registry patch missing: " + ", ".join(missing))

    PATCH.write_text(text, encoding="utf-8")
    print("Prepared Zombie Shooter 3.6.1 direct Registry file backend")


if __name__ == "__main__":
    main()
