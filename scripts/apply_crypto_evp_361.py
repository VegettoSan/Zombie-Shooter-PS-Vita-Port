#!/usr/bin/env python3
"""Inject the narrowly-scoped Zombie Shooter 3.6.1 EVP compatibility hook.

Real-Vita logs from build #38 prove the Registry reaches SharedPreferences but
AES-256 encryption fails after the guest has already validated a 32-byte key.
Build-1161 disassembly shows the observed `Wrong AES key length` is emitted only
after EVP_CipherInit_ex when EVP_CIPHER_CTX_get_key_length(ctx) does not report
32. The AES wrapper then performs the same kind of check for the fixed 16-byte
CBC IV.

The Vita OpenSSL/provider state can therefore disagree with the guest's own
already-validated AES-256-CBC contract. This patch does NOT alter android_id,
the SHA-256-derived key, ciphertext, or OpenSSL globally. It hooks only the two
metadata getters and overrides their result only when the caller is one of the
exact encrypt/decrypt metadata checks in Zombie Shooter 3.6.1 build 1161.
The second EVP_CipherInit_ex (with the real key/IV) remains untouched and its
return value is still checked by the game, so a genuinely unusable context
continues to fail safely.

Source drift aborts instead of applying a blind patch. The runtime installer
also validates exact exported symbol addresses, Thumb state and function
prologues before installing either hook. The normalized/even address is used
only for byte verification; the original odd ELF symbol is passed to hook_addr
so so_util installs a Thumb hook rather than an ARM hook.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PATCH = ROOT / "source/patch.c"
MARKER = "ZS361_EVP_METADATA_COMPAT"

HOOK_CODE = r'''
/* ZS361_EVP_METADATA_COMPAT
 *
 * Build 1161's Chipher<encrypt/decrypt,256>::porcess validates key.size()==32
 * before these calls, selects EVP_aes_256_cbc(), and constructs a 16-byte IV.
 * On Vita the OpenSSL 3 provider-backed context can fail to publish that
 * metadata after the first EVP_CipherInit_ex even though the guest inputs are
 * already correct. Keep the workaround exact-call-site only; the subsequent
 * EVP_CipherInit_ex with actual key+IV is NOT bypassed and remains authoritative.
 *
 * IMPORTANT: both target OpenSSL functions are Thumb symbols. so_util's
 * hook_addr() selects hook_thumb() only when bit 0 of the target address is set.
 * Normalize the address only while validating the build-1161 entry/prologue;
 * preserve the original odd symbol value when installing the hook.
 */
static so_hook zs361_evp_key_length_hook;
static so_hook zs361_evp_iv_length_hook;

#define ZS361_EVP_KEYLEN_ENTRY          0x0059A8E0u
#define ZS361_EVP_IVLEN_ENTRY           0x00599C78u
#define ZS361_AES_ENC_KEYLEN_RETURN     0x00408626u
#define ZS361_AES_ENC_IVLEN_RETURN      0x00408632u
#define ZS361_AES_DEC_KEYLEN_RETURN     0x00408C04u
#define ZS361_AES_DEC_IVLEN_RETURN      0x00408C10u

static uintptr_t zs361_normalize_thumb(uintptr_t address) {
    return address & ~(uintptr_t)1u;
}

static int zs361_crypto_caller_is(uintptr_t caller, uintptr_t a, uintptr_t b) {
    caller = zs361_normalize_thumb(caller);
    return caller == so_mod.load_addr + a || caller == so_mod.load_addr + b;
}

static int zs361_hooked_evp_key_length(void *ctx) {
    uintptr_t caller = zs361_normalize_thumb((uintptr_t)__builtin_return_address(0));
    int actual = SO_CONTINUE(int, zs361_evp_key_length_hook, ctx);
    if (zs361_crypto_caller_is(caller,
                               ZS361_AES_ENC_KEYLEN_RETURN,
                               ZS361_AES_DEC_KEYLEN_RETURN)) {
#ifdef ZOMBIE_DEBUG_BUILD
        static unsigned reports;
        if (__atomic_fetch_add(&reports, 1, __ATOMIC_RELAXED) < 32)
            l_perf("[CRYPTO] EVP AES256 keylen caller=so+0x%08X actual=%d expected=32 forced=%d",
                   (unsigned)(caller - so_mod.load_addr), actual, actual != 32);
#endif
        return 32;
    }
    return actual;
}

static int zs361_hooked_evp_iv_length(void *ctx) {
    uintptr_t caller = zs361_normalize_thumb((uintptr_t)__builtin_return_address(0));
    int actual = SO_CONTINUE(int, zs361_evp_iv_length_hook, ctx);
    if (zs361_crypto_caller_is(caller,
                               ZS361_AES_ENC_IVLEN_RETURN,
                               ZS361_AES_DEC_IVLEN_RETURN)) {
#ifdef ZOMBIE_DEBUG_BUILD
        static unsigned reports;
        if (__atomic_fetch_add(&reports, 1, __ATOMIC_RELAXED) < 32)
            l_perf("[CRYPTO] EVP AES256 ivlen caller=so+0x%08X actual=%d expected=16 forced=%d",
                   (unsigned)(caller - so_mod.load_addr), actual, actual != 16);
#endif
        return 16;
    }
    return actual;
}

static int zs361_crypto_symbol_matches(const char *name, uintptr_t expected_offset,
                                       const uint8_t expected_prologue[8],
                                       uintptr_t *hook_target_out) {
    uintptr_t symbol = so_symbol(&so_mod, name);
    uintptr_t entry = zs361_normalize_thumb(symbol);
    uintptr_t expected = so_mod.load_addr + expected_offset;
    if (!symbol || entry != expected) {
        l_error("[CRYPTO] refusing EVP hook %s: symbol=%p expected=so+0x%08X",
                name, (void *)symbol, (unsigned)expected_offset);
        return 0;
    }
    if ((symbol & (uintptr_t)1u) == 0) {
        l_error("[CRYPTO] refusing EVP hook %s: build-1161 symbol lost Thumb bit", name);
        return 0;
    }
    if (memcmp((const void *)entry, expected_prologue, 8) != 0) {
        l_error("[CRYPTO] refusing EVP hook %s: build-1161 prologue mismatch", name);
        return 0;
    }
    *hook_target_out = symbol;
    return 1;
}

static void install_zs361_crypto_evp_compat(void) {
    static const uint8_t key_prologue[8] = {0xF0,0xB5,0x03,0xAF,0x4D,0xF8,0x04,0xBD};
    static const uint8_t iv_prologue[8]  = {0xB0,0xB5,0x02,0xAF,0x8E,0xB0,0x04,0x46};
    uintptr_t key_target = 0, iv_target = 0;

    if (!zs361_crypto_symbol_matches("EVP_CIPHER_CTX_get_key_length",
                                     ZS361_EVP_KEYLEN_ENTRY,
                                     key_prologue, &key_target) ||
        !zs361_crypto_symbol_matches("EVP_CIPHER_CTX_get_iv_length",
                                     ZS361_EVP_IVLEN_ENTRY,
                                     iv_prologue, &iv_target)) {
        l_error("[CRYPTO] 3.6.1 EVP metadata compatibility disabled; exact guards failed");
        return;
    }

    zs361_evp_key_length_hook = hook_addr(key_target,
                                          (uintptr_t)&zs361_hooked_evp_key_length);
    zs361_evp_iv_length_hook = hook_addr(iv_target,
                                         (uintptr_t)&zs361_hooked_evp_iv_length);
    l_info("[CRYPTO] installed guarded 3.6.1 AES-256 EVP metadata compatibility (Thumb-safe)");
}
'''


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"patch.c: expected one {label} anchor, found {count}; refusing drift")
    return text.replace(old, new, 1)


def main() -> None:
    text = PATCH.read_text(encoding="utf-8")
    if MARKER in text:
        # Idempotence: also prove the installer call survived.
        if text.count("install_zs361_crypto_evp_compat();") != 1:
            raise SystemExit("patch.c: crypto marker exists but installer call is missing/duplicated")
        print("3.6.1 guarded EVP metadata compatibility already prepared")
        return

    text = replace_once(
        text,
        "static so_hook registry_load_value_hook;\n",
        "static so_hook registry_load_value_hook;\n" + HOOK_CODE + "\n",
        "registry hook declaration",
    )
    text = replace_once(
        text,
        "\tkuser_patch();\n",
        "\tkuser_patch();\n\tinstall_zs361_crypto_evp_compat();\n",
        "kuser_patch installer",
    )

    # Static source gates: these exact values were independently disassembled
    # from the user's canonical 3.6.1 ARMv7 binary. The hook-target checks also
    # ensure the generated code keeps the ELF Thumb bit instead of passing the
    # normalized/even address into so_util's architecture-dispatching hook_addr.
    required = [
        "0x0059A8E0u", "0x00599C78u",
        "0x00408626u", "0x00408632u",
        "0x00408C04u", "0x00408C10u",
        "EVP_CIPHER_CTX_get_key_length", "EVP_CIPHER_CTX_get_iv_length",
        "*hook_target_out = symbol;",
        "hook_addr(key_target", "hook_addr(iv_target",
        "install_zs361_crypto_evp_compat();",
    ]
    missing = [token for token in required if token not in text]
    if missing:
        raise SystemExit("patch.c: generated crypto patch missing: " + ", ".join(missing))

    PATCH.write_text(text, encoding="utf-8")
    print("Prepared guarded Zombie Shooter 3.6.1 AES-256 EVP metadata compatibility")


if __name__ == "__main__":
    main()
