# Zombie Shooter 3.6.1 — save / crypto findings

Date: 2026-09-28
Status: root cause narrowed by real-Vita logs + build-1161 disassembly; fix requires physical verification.

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
[SAVE] open path=ux0:data/zombieshooter/shared_preferences.bak mode=rb ok=0 errno=2
[SAVE] Activity.getPreferences keys=0 path=ux0:data/zombieshooter/shared_preferences.bin
```

However, the run contains hundreds of operations equivalent to:

```text
[SAVE] durable remove hash=... ok=1
[SAVE] apply ok=1 keys=0
```

There are no successful non-null preference values and the registry stays at zero keys. The same run contains roughly 299 instances of:

```text
[AES] Wrong AES key length
bool core::crypto::Chipher<1, 256>::porcess(...)
aes256.cpp:156
```

The AES errors and zero-key remove/apply sequence are strongly interleaved. Therefore the missing file is **not evidence that SharedPreferences is never reached**. Instead, the native Registry encryption fails before a usable string value reaches `Editor.putString`; the null result is then observed as removal semantics, leaving no dirty non-empty store to persist.

## Exact native key derivation in 3.6.1 build 1161

Target native library:

```text
libzombie_shooter.so
version 3.6.1 / versionCode 1161
armeabi-v7a
SHA256 cb461ac47de79536824e8f7b64fa82293b796bcc159675b4f5ad6304e60bdca1
```

Thumb disassembly and relocations show this chain:

```text
core::CryptEngine::encryptionKey()
  -> CryptEngine::salt()
  -> CryptEngine::applicationUID()
  -> Activity.getContentResolver()
  -> android.provider.Settings$Secure.getString(resolver, "android_id")
  -> native STRING
  -> core::crypto::sha256(c_str, length)
  -> 32-byte digest
  -> ICryptEngine::generateKey(...)
  -> AES-256 encryption
  -> Registry string
  -> SharedPreferences
```

### Important: do NOT make Android ID 32 characters

`Settings.Secure.ANDROID_ID` is correctly emulated as a stable 16-character hexadecimal identifier:

```text
a1b2c3d4e5f60718
```

The engine itself SHA-256 hashes that value. A valid 16-byte ASCII Android ID therefore produces the required 32-byte digest.

Changing the Java result to a 32-character/pre-hashed key would change SigmaTeam's real key derivation contract and is prohibited unless new disassembly proves otherwise.

### Why an empty Android ID breaks exactly as observed

`core::crypto::sha256` in build 1161 special-cases null/zero-length input by returning an **empty byte vector**, not SHA256(empty).

`Chipher<1,256>::porcess` then computes the byte-vector size and explicitly requires:

```text
key.size() == 0x20
```

If not, it emits the exact hardware message:

```text
Wrong AES key length
```

Thus a null/empty value from the Java identity path is sufficient to explain the hardware failure.

## JNI compatibility bug

The old FalsoJNI compatibility path primarily resolves methods by name. That is unsafe when the native engine expects exact Java descriptors.

For the save crypto path, build 1161 expects exactly:

```text
Activity.getContentResolver()
  ()Landroid/content/ContentResolver;

android.provider.Settings$Secure.getString(
  Landroid/content/ContentResolver;
  Ljava/lang/String;
)Ljava/lang/String;
```

The 3.6.1 preparation now explicitly intercepts those class/name/signature combinations before legacy FalsoJNI lookup.

It also consumes wrong signatures and returns `NULL`, so an incorrect descriptor cannot silently resolve by method name alone.

The synthetic resolver object is no longer used as a hard rejection criterion inside `secureGetString()`. Once the exact static API is resolved, only the requested key name matters; `android_id` returns the stable ID and unrelated Settings.Secure names return null.

## Host regression gate

`tests/input_device_regression.c` follows the same identity route the native `applicationUID()` uses. It must prove:

- exact `getContentResolver` descriptor resolves;
- wrong descriptor does not resolve;
- exact `Settings$Secure.getString(ContentResolver,String)` resolves;
- wrong descriptor does not resolve;
- `android_id` returns non-null;
- value is exactly the stable 16-character hex ID;
- unrelated secure keys return null.

Expected host message:

```text
Crypto identity JNI regression passed: exact Settings.Secure android_id path and stable non-empty UID
```

A build must stop if this regression fails.

## Expected next real-Vita evidence

In the next Debug run, a working Java identity path should log something equivalent to:

```text
[CRYPTO] Settings.Secure.getString name=android_id ... android_id=1
```

Then the previous repeated AES error should disappear. The save bridge should begin seeing non-null values, for example:

```text
[SAVE] preference put hash=... bytes=>0 ok=1
[SAVE] write path=ux0:data/zombieshooter/shared_preferences.tmp keys=>0 ok=1
[SAVE] apply ok=1 keys=>0
```

and VitaShell should show:

```text
ux0:data/zombieshooter/shared_preferences.bin
```

Only after completing progress, fully closing the game, relaunching and observing restored campaign state may this be marked `REAL VITA VERIFIED / GAMEPLAY VERIFIED`.

## Decision tree if the next test still fails

If `[CRYPTO] ... android_id=1` appears but `Wrong AES key length` remains, the Java identity layer is no longer the culprit. Instrument the output size of native `applicationUID()` / `encryptionKey()` with a guarded build-1161-specific hook and verify the vector size before AES.

If `[CRYPTO]` never appears, native `applicationUID()` is not reaching the emulated Java call (or an empty value was cached earlier). Trace exact method-resolution/call order and, if necessary, guard-instrument `applicationUID()` itself.

If AES errors disappear and non-null `[SAVE] preference put` events appear but relaunch still loses progress, continue with Registry batching/flush/load semantics (`beginBatchUpdate`, `endBatchUpdate`, `setRegSync`, `setRegAsync`, `queueFlush`, dump/load) rather than changing AES or Android ID again.

If `shared_preferences.bin` is written but native code sees a wrong size/existence, inspect the already-corrected Android/Bionic `stat/fstat` ABI and its Debug traces before changing storage format.

## Safety / rollback

Pre-fix backup:

```text
backup/pre-save-crypto-fix-20260928
2944b377f12d3713fa4fc1c6d2d498779cb7db17
```

Do not remove this branch.

No save/crypto change is considered fixed merely because host regressions and VPK builds pass. Physical Vita evidence is authoritative.
