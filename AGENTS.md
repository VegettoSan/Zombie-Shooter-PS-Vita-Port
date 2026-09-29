# AGENTS.md — Zombie Shooter PS Vita Port

These instructions apply to the entire repository and are the first handoff for Codex/AI-assisted work.

## Active target

Port the Android version of **Zombie Shooter** to **real PS Vita hardware** by loading/adapting the original ARM Android library rather than reimplementing the game.

Canonical target:

```text
Zombie Shooter Free 3.6.1
versionCode: 1161
package: com.sigmateam.zombieshooter.free
library: libzombie_shooter.so
ABI: armeabi-v7a / ARM32 EABI5 SoftFP
size: 9,908,972 bytes
SHA1: f7c7bbfc41f7ed8b76c8c5b1af3e9b0dfdf188c0
SHA256: cb461ac47de79536824e8f7b64fa82293b796bcc159675b4f5ad6304e60bdca1
Vita data: ux0:data/zombieshooter/
```

**Never use 3.2.3/3.5.3 offsets, hashes, Java assumptions or native layouts as if they belonged to 3.6.1.** Older work is historical reference only unless revalidated against this exact binary.

## Mandatory reading before code changes

For boot, JNI, filesystem, save, input, renderer, shader, asset, audio, performance, crash or loader work, read:

```text
1. AGENTS.md
2. docs/CURRENT_ENGINEERING_STATE_2026-09-29.md
3. docs/METALSYNTAX_PORTING_GUIDE.md
4. docs/METALSYNTAX_TOOLKIT_FINDINGS_2026-09-28.md
5. newest relevant real-Vita log/report for the subsystem
```

`docs/CURRENT_ENGINEERING_STATE_2026-09-29.md` supersedes older blocker/status documents when they conflict. It records the gameplay-verified save architecture and the build-66 asset-I/O regression that must not be repeated.

## Current real-Vita baseline

Preferred stable physical-test reference:

```text
source state: 6b9ad638c768dd14945ef318a85e7bf7200bcdd9
physical-test build: 59
Release VPK SHA256: 2898570a8b5c64a84d7aed3a77e87be8a64448ea7c5a10fae76dc172c1e6c836
```

A later direct-AAsset experiment (build 66) was rejected. After rollback, `master` source contents were compared against `6b9ad638...` and GitHub reported zero changed files. Later rollback/documentation commits are history; functionally the code returned to the build-59 baseline before documentation-only changes.

Verified current state:

```text
campaign save/load across relaunch: GAMEPLAY VERIFIED
save-induced ~1 s gameplay freeze: FIXED / GAMEPLAY VERIFIED via async worker
quiet logger physical-fsync stalls: FIXED
build-66 direct sceIo AAsset backend: REGRESSION / REJECTED / ROLLED BACK
initial loading time: still too slow; optimize only with new measurements
next active engineering area: shaders/render correctness + measured performance
```

Do not overwrite verified behavior merely because an older document says SAVE still fails.

## Real hardware is authoritative

The authoritative runtime is a **physical PS Vita**. Vita3K is useful but is not authoritative for this kubridge/soloader path.

Use SoftFP VitaSDK only:

```bash
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"
"$VITASDK/bin/arm-vita-eabi-gcc" -Q --help=target | grep float
```

It must report `-mfloat-abi=softfp`.

Never modify `/usr/local/vitasdk-hardfp` and never reuse a build directory created with another ABI/toolchain.

## Source of truth and patch discipline

The exact 3.6.1 APK/XAPK/DEX and ARMv7 `.so`, plus real Vita logs/dumps, are the source of truth.

Use as appropriate:

```text
AndroidManifest.xml
JADX / smali
readelf / nm -D -C / objdump
strings
Ghidra / targeted decompilation
real Vita logs
.psp2dmp
```

Every game-specific hook/patch must be tied to this exact binary by strong identity evidence such as symbol/address, prologue/signature guard, disassembly/decompilation evidence and/or hardware evidence. Prefer multiple guards. Never assume constant deltas between versions. Fail closed when a guard does not match.

## JNI / Bionic ABI invariant

Treat JNI signatures and Android guest layouts as correctness-critical. Verify static vs instance, receiver type, argument count/types, return type, object/string/array representation and ARM32 packing/alignment.

`source/reimpl/io.h` intentionally models ARM32 Bionic filesystem layouts with fixed-width fields and assertions. Do not replace them with Vita/newlib host typedefs without proving binary identity. `stat_newlib_to_bionic()` and `dirent_newlib_to_bionic()` must populate guest layouts field-by-field.

## Save system — protected verified behavior

Campaign persistence is **GAMEPLAY VERIFIED** on real Vita.

Current authoritative format:

```text
ux0:data/zombieshooter/save/<encrypted-key-hex>.dat
```

Each file stores one raw Registry value. The native `Registry::encryptKey()` path preserves the game's key namespace. Do not replace this with an invented aggregate container unless new binary evidence proves the game requires it.

Important exact-3.6.1 symbols:

```text
core::Registry::storeEncrypted  0x003FB985 (Thumb)
core::Registry::loadDecrypted   0x003FB055 (Thumb)
core::Registry::encryptKey      0x003FBFF5 (Thumb)
core::Registry::remove          0x003FBEF5
core::Registry::contains        0x003FD351
STRING::c_str                   0x003ED769 (Thumb)
```

The original AES value-encryption path was not safe in this environment; EVP initialization failed. Do not revive it without proving the exact key/context contract.

### Save hot-path invariant

Build 52 proved that synchronous filesystem persistence can visibly freeze gameplay. Build 59 fixes it with an asynchronous worker:

```text
Registry mutation
-> synchronously update RAM map
-> enqueue owned mutation
-> return to game
-> worker performs tmp/bak/dat persistence
```

Preserve:

- immediate RAM visibility for reads;
- queue-owned copies of key/value data;
- coalescing of repeated updates to the same key;
- correct clear/remove ordering;
- safe synchronous fallback if worker creation fails;
- existing per-key filenames/format;
- no device-wide `sceIoSync("ux0:", ...)` per value.

Do **not** move physical save writes back onto the gameplay/render thread.

## Logger — protected performance invariant

Hardware logs showed physical log syncs can cost hundreds of milliseconds to over one second.

Quiet Debug/Release checkpoints may flush buffered bytes but must not call physical `sceIoSyncByFd()` for routine ERROR/PERF/time thresholds. Fatal and explicit `logger_force_sync()` remain durable sync points. Verbose diagnostic mode may intentionally be more synchronous.

Do not reintroduce physical fsync per error, per save or periodic quiet checkpoint.

## Asset/loading I/O — explicit rejected experiment

### Build 66 direct `sceIo` AAsset backend is REJECTED

Do **not** recreate the build-66 approach as-is.

It replaced buffered FalsoNDK/stdio AAsset handling with direct:

```text
sceIoGetstat -> sceIoOpen -> sceIoRead/sceIoLseek -> sceIoClose
```

The hypothesis came from aggregate `AAssetManager_open()` timing. Real Vita testing showed a severe regression:

```text
initial loading worse
menu transitions much slower
level transitions much slower
~94 s startup/VID gap in the captured Release log
additional ~20–25 s transition delays
```

Likely lesson: Zombie Shooter performs many small logical reads while parsing VID/maps/menus. `fread` provides useful stdio buffering. Direct unbuffered `sceIoRead` can turn those small reads into physical filesystem operations, making the system slower despite avoiding seek/ftell work during open.

**Hard rule:** never replace current buffered AAsset reads with direct unbuffered `sceIoRead` based only on aggregate open time.

Any future loading optimization must A/B all of:

```text
cold launch -> main menu
main menu -> gameplay
pause/options/menu transitions
level/zone transitions
gameplay -> main menu
```

Preserve buffering/read-ahead. Measure read size/count patterns before changing the backend. Do not aggressively increase the asset cache without real-Vita memory headroom measurements.

## Input

3.6.1 contains a native joystick path (`AndroidJoystickControl`, `InputHandlerNative::onJoysticEvent`, motion-axis handling).

Preferred architecture:

```text
Vita controls
-> correct Android AInputEvent source/device/key/axis semantics
-> game's native joystick path
```

Use synthetic touch only for actions proven touch-driven. Do not layer translations blindly. Verify analog and digital paths separately on hardware.

## Graphics and shaders — next active area

Keep VitaGL as compatibility baseline until evidence supports a change.

The repository already has shader cache/inventory instrumentation. Previous logs showed shader-cache hits and no evidence that shader compilation alone explains multi-minute loading. Shader work should therefore focus on correctness and measured cost, not assume it will solve loading automatically.

Required workflow:

```text
observe exact glShaderSource input
-> hash/deduplicate
-> map programs actually used in gameplay
-> dump unknown/problematic shaders in Debug only
-> transpile offline with real tooling where justified
-> psp2cgc/toolchain validate
-> A/B visual correctness and FPS on real Vita
```

Useful reference pipeline:

```text
GLSL ES -> glslangValidator -> SPIR-V -> SPIRV-Cross -> Cg -> psp2cgc
```

Do not promise gameplay FPS gains from shader precompilation alone; the game also has expensive software rendering/full-frame upload work.

## Performance discipline

Optimize only measured active work on physical hardware.

Prefer eliminating work before micro-optimizing it:

```text
redundant framebuffer/texture uploads
redundant copies/conversions
repeated decode/compile work
failed path probes
unnecessary synchronization
excessive Release logging
```

Use Release for timing/FPS comparisons. Debug is for bounded evidence collection. Aggregate counters are hypotheses, not proof of root cause; build 66 is the concrete example.

Within a subsystem use:

```text
evidence
-> small hypothesis
-> small change
-> build/regression
-> physical Vita A/B test
-> new evidence
-> root cause
```

Avoid unrelated speculative patch stacks.

## Audio

Preserve the recovered 3.6.1 music/audio path unless exact evidence shows a defect. Do not replace working audio architecture casually; identify the engine/OpenSL/player path first.

## Lifecycle

3.6.1 follows a NativeActivity-style path and exports `ANativeActivity_onCreate`. Preserve Android lifecycle ordering and keep loader stages observable:

```text
load ELF
-> relocate
-> resolve imports
-> guarded compatibility patches
-> cache maintenance where required
-> constructors/init array
-> real NativeActivity/game lifecycle
```

## Safety backup

Pre-MetalSyntax integration backup:

```text
backup/pre-metalsyntax-integration-20260928
commit 24d8d4c2db27bd6d953db37ac881ea3e56ebd7b1
```

Do not delete or move it.

## Repository/publication policy

Normal Codex work is local unless the user explicitly authorizes remote changes in the active task. Do not push, publish releases, force-update branches or rewrite history without explicit authorization. Do not use destructive git operations such as `reset --hard` or `clean -fd` as a shortcut.

## Build

Debug:

```bash
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
```

Release:

```bash
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

The 3.6.1 GitHub Actions workflow builds/regresses Debug and Release and publishes physical-test prereleases after successful code pushes. Documentation-only changes do not prove runtime behavior.

## Verification vocabulary

Always distinguish:

```text
STATICALLY VERIFIED
BUILD VERIFIED
VPK VERIFIED
REAL VITA VERIFIED
GAMEPLAY VERIFIED
REGRESSION / REJECTED
```

Never call something fixed merely because it compiles.

## Current engineering order

Unless newer real-Vita evidence changes priorities:

```text
1. preserve build-59 save/input/audio/render baseline
2. shader/render investigation and correctness
3. measure shader/runtime cost on real Vita
4. improve only measured shader/render bottlenecks
5. revisit loading later with buffered-I/O-aware instrumentation
6. never retry build-66 direct unbuffered AAsset approach without a fundamentally different design and new evidence
```

## End-of-task report

Keep reports concise and include:

```text
exact target/version/hash
source evidence used
root cause or remaining hypothesis
files changed
regression/build status
Debug/Release/VPK identity where relevant
verification level
exact physical-Vita test matrix
at most 1–3 requested logs/dumps
```
