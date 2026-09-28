# Zombie Shooter 3.6.1 — save / crypto findings

Date: 2026-09-28
Status: root cause narrowed by real-Vita logs + exact build-1161 disassembly; EVP compatibility fix is BUILD-PENDING / REAL-VITA-PENDING.

This is a permanent handoff for humans, ChatGPT and Codex. Read it before changing Registry, SharedPreferences, Android identity, AES, `stat/fstat`, save paths or JNI identity code.

## Physical evidence from build #38

The user tested both Release and Debug from `vita-361-test-38-2944b37` on a real PS Vita.

Observed behavior:

- campaign progress did not persist;
- relaunch returned to the tutorial;
- `ux0:data/zombieshooter/shared_preferences.bin` was not created;
- `.tmp` / `.bak` save generations were not created either.

The Debug log proves the game **does reach our SharedPreferences bridge**:

```text
[SAVE] RegistryEnumerator.onKey registered
[SAVE] resolve Activity.getPreferences(I) receiver=0x42424242
[SAVE] open path=ux0:data/zombieshooter/shared_preferences.bin mode=rb ok=0 errno=2
[SAVE] Activity.getPreferences keys=0 path=ux0:data/zombieshooter/shared_preferences.bin
```

The same run repeatedly shows operations equivalent to:

```text
[SAVE] durable remove hash=... ok=1
[SAVE] apply ok=1 keys=0
```

interleaved with roughly 299 occurrences of:

```text
[AES] Wrong AES key length
bool core::crypto::Chipher<1, 256>::porcess(...)
aes256.cpp:156
```

Therefore the missing file is **not evidence that SharedPreferences is never reached**. Native Registry encryption fails before a usable encrypted string reaches `Editor.putString`, leaving the registry at zero useful keys.

## Exact native key derivation in 3.6.1 build 1161

Target library:

```text
libzombie_shooter.so
version 3.6.1 / versionCode 1161
armeabi-v7a
SHA256 cb461ac47de79536824e8f7b64fa82293b796bcc159675b4f5ad6304e60bdca1
```

Disassembly shows:

```text
CryptEngine::encryptionKey()
  -> CryptEngine::salt()
  -> CryptEngine::applicationUID()
  -> Settings.Secure.getString(..., "android_id")
  -> STRING
  -> SHA-256
  -> 32-byte key vector
  -> Chipher<encrypt,256>::porcess
  -> EVP_aes_256_cbc / EVP_CipherInit_ex
  -> Registry encrypted string
  -> SharedPreferences
```

### Do NOT make Android ID 32 characters

The emulated Android ID remains a stable 16-character hexadecimal identifier:

```text
a1b2c3d4e5f60718
```

The engine hashes the identity itself with SHA-256. Changing it to a pre-hashed or 32-character fake key changes the original SigmaTeam contract and is prohibited without new binary evidence.

The exact JNI descriptors for `Activity.getContentResolver()` and `Settings$Secure.getString(ContentResolver,String)` are explicitly handled by the 3.6.1 compatibility profile. Wrong signatures are rejected instead of falling through to name-only lookup.

## Refined root cause: observed error is AFTER the native 32-byte key check

Deeper build-1161 disassembly corrects an earlier interpretation of the error text.

`Chipher<1,256>::porcess` first checks the actual C++ key vector:

```text
so+0x004085DC  load key begin/end
so+0x004085E0  subtract
so+0x004085E2  cmp #0x20
```

That earlier branch has a different error path. The **hardware message** `Wrong AES key length` at `aes256.cpp:156` is emitted later, after the first `EVP_CipherInit_ex`, when this call does not report 32:

```text
so+0x00408622  EVP_CIPHER_CTX_get_key_length(ctx)
so+0x00408626  cmp #0x20
```

The decrypt path has the same metadata check:

```text
so+0x00408C00  EVP_CIPHER_CTX_get_key_length(ctx)
so+0x00408C04  cmp #0x20
```

Therefore the observed hardware error proves the guest already passed its own 32-byte key-vector validation. The failure is the **OpenSSL EVP context metadata query on Vita**, not the length of `android_id` and not the SHA-256 output vector.

This is consistent with the previous controlled native crypto regression: the original key derivation with the stable 16-character UID produces a 32-byte key and the original encrypt/decrypt path round-trips correctly under the controlled host fixture. Vita's runtime/provider state is the remaining difference.

## IV metadata has the same provider-backed pattern

Encrypt constructs a fixed 16-byte CBC IV (`0xA0` repeated) and then checks:

```text
so+0x0040862E  EVP_CIPHER_CTX_get_iv_length(ctx)
so+0x00408632  cmp #0x10
```

Decrypt checks:

```text
so+0x00408C0C  EVP_CIPHER_CTX_get_iv_length(ctx)
so+0x00408C10  cmp #0x10
```

The guest has already selected `EVP_aes_256_cbc()`, for which key/IV sizes are fixed at 32/16 bytes.

## Guarded compatibility fix

`scripts/apply_crypto_evp_361.py` injects a build-1161-only compatibility layer into `source/patch.c` at build time. `scripts/prepare_build.py` runs it for both local CMake builds and GitHub Actions.

The patch:

1. resolves `EVP_CIPHER_CTX_get_key_length` and `EVP_CIPHER_CTX_get_iv_length` from the loaded guest `.so`;
2. requires their exact build-1161 normalized entries:

```text
EVP_CIPHER_CTX_get_key_length  so+0x0059A8E0
EVP_CIPHER_CTX_get_iv_length   so+0x00599C78
```

3. verifies the exact first 8 bytes of each function before installing hooks;
4. calls the original getter first;
5. overrides only the four exact AES-256 metadata call sites listed above;
6. returns 32 for the key metadata query and 16 for the IV metadata query only at those call sites;
7. leaves every other OpenSSL caller untouched.

Most importantly, the **second `EVP_CipherInit_ex` with the actual key and IV is not bypassed**. Zombie Shooter checks its return value. If the context is genuinely unable to perform AES-256-CBC, encryption still fails naturally instead of being falsely reported as successful.

This makes the workaround substantially narrower than globally stubbing OpenSSL metadata or replacing the game's encryption algorithm.

## Runtime Debug evidence expected from the patched build

A successful hook installation should report:

```text
[CRYPTO] installed guarded 3.6.1 AES-256 EVP metadata compatibility
```

At encryption/decryption metadata checks Debug can report, bounded to a small number of lines:

```text
[CRYPTO] EVP AES256 keylen caller=so+0x00408626 actual=... expected=32 forced=...
[CRYPTO] EVP AES256 ivlen  caller=so+0x00408632 actual=... expected=16 forced=...
```

If the second `EVP_CipherInit_ex` succeeds, the expected save progression is then:

```text
[SAVE] preference put hash=... bytes=>0 ok=1
[SAVE] write path=ux0:data/zombieshooter/shared_preferences.tmp keys=>0 ok=1
[SAVE] apply ok=1 keys=>0
```

and VitaShell should show:

```text
ux0:data/zombieshooter/shared_preferences.bin
```

Only after campaign progress is restored following a full close/relaunch may save support be marked `REAL VITA VERIFIED / GAMEPLAY VERIFIED`.

## Decision tree after the EVP test

If the old `Wrong AES key length` disappears but the next failure is `Wrong AES IV length`, verify the IV hook installation/caller value; do not alter the IV data.

If key/IV metadata checks pass but the second `EVP_CipherInit_ex` returns failure, inspect `ERR_peek_last_error` and provider/context initialization. Do not bypass that failure.

If AES succeeds and non-null preference puts appear but relaunch still loses progress, continue with Registry batching/flush/load semantics (`beginBatchUpdate`, `endBatchUpdate`, `setRegSync`, `setRegAsync`, `queueFlush`, dump/load) rather than changing AES again.

If `shared_preferences.bin` is written but native code sees a wrong size/existence, inspect the already-corrected Android/Bionic `stat/fstat` ABI and its Debug traces before changing the storage format.

## Safety / rollback

Pre-save-crypto backup:

```text
backup/pre-save-crypto-fix-20260928
2944b377f12d3713fa4fc1c6d2d498779cb7db17
```

Earlier MetalSyntax integration backup:

```text
backup/pre-metalsyntax-integration-20260928
24d8d4c2db27bd6d953db37ac881ea3e56ebd7b1
```

Do not remove either branch.

No save/crypto change is considered fixed merely because host regressions and VPK builds pass. Physical Vita evidence remains authoritative.
