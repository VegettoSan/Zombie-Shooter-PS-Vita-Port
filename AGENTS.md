# AGENTS.md — Zombie Shooter PS Vita Port

These instructions apply to the entire repository and are the first handoff for Codex/AI-assisted work.

## Project goal and active target

Port the Android version of **Zombie Shooter** to **real PS Vita hardware** by loading and adapting the original Android ARM shared library rather than reimplementing the game.

The **active target is now Zombie Shooter 3.6.1 build 1161, `armeabi-v7a`**:

```text
package: com.sigmateam.zombieshooter.free
library: libzombie_shooter.so
ABI: ARM32 EABI5 soft-float
size: 9,908,972 bytes
SHA1: f7c7bbfc41f7ed8b76c8c5b1af3e9b0dfdf188c0
SHA256: cb461ac47de79536824e8f7b64fa82293b796bcc159675b4f5ad6304e60bdca1
Vita data path: ux0:data/zombieshooter/
```

**Do not use 3.2.3/3.5.3 offsets, hashes or Java assumptions as if they belonged to 3.6.1.** Older material remains useful only as historical architecture/performance reference.

## Mandatory reading before code changes

For any Android->Vita boot, JNI, filesystem, save, input, renderer, shader, asset, audio, performance, crash or loader task, read in this order:

```text
1. AGENTS.md
2. docs/METALSYNTAX_PORTING_GUIDE.md
3. docs/METALSYNTAX_TOOLKIT_FINDINGS_2026-09-28.md
4. the newest relevant hardware report/log for the subsystem being changed
```

`docs/METALSYNTAX_TOOLKIT_FINDINGS_2026-09-28.md` is the current 3.6.1 engineering handoff. It documents the corrected Android/Bionic filesystem ABI, Registry/save investigation, MetalSyntax toolkit usage, shader plan and performance methodology.

Old blocker documents remain useful history, but the 3.6.1 handoff and newer physical-Vita evidence supersede conflicting older assumptions.

## Current physical-Vita state

The first 3.6.1 bring-up was playable, but the user reported:

```text
~10 FPS regression versus the prior optimized baseline
music missing
controls still wrong
campaign save still not persistent
```

A recovery build later re-enabled only 3.6.1-rederived music/render-scale/frame-reuse hooks and revised save/input handling. **Do not claim those recovered behaviors fixed until the user tests the current build on a physical Vita.**

Known user-visible priorities remain:

```text
1. campaign save/load persistence
2. correct physical/digital controller behavior
3. recover/improve FPS without visual regressions
4. stable music/audio
5. texture/sprite correctness and loading time
```

## Safety backup for the current engineering pass

Before the MetalSyntax-toolkit/ABI integration, `master` was preserved at:

```text
backup/pre-metalsyntax-integration-20260928
commit 24d8d4c2db27bd6d953db37ac881ea3e56ebd7b1
```

Do not delete or move that backup.

## Source-of-truth rule

The exact 3.6.1 XAPK/APK/DEX and exact ARMv7 `.so` are the source of truth.

Use, as appropriate:

```text
AndroidManifest.xml
JADX / smali
readelf
nm -D -C
objdump -d/-T
strings
Ghidra / targeted decompilation
real Vita logs
.psp2dmp
```

Never infer behavior only from names or from another port.

## Target hardware and VitaSDK

The authoritative runtime is a **real PS Vita**. Vita3K is not authoritative for this kubridge/soloader path.

Use the SoftFP VitaSDK only:

```bash
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"
"$VITASDK/bin/arm-vita-eabi-gcc" -Q --help=target | grep float
```

It must report `-mfloat-abi=softfp`.

Never modify `/usr/local/vitasdk-hardfp`.

Never reuse a CMake build directory created with another ABI/toolchain.

## Lifecycle

3.6.1 uses the NativeActivity-style path and exports `ANativeActivity_onCreate`; the loader currently drives the game through FalsoNDK/FalsoJNI and its Android event loop.

Preserve semantic Android lifecycle ordering. If changing bootstrap behavior, verify it against the exact 3.6.1 Java/native path first.

Loader stages should remain observable:

```text
load ELF
-> relocate
-> resolve imports
-> apply individually justified compatibility patches
-> cache maintenance if required
-> constructors/init array
-> real NativeActivity/game lifecycle
```

## Version-specific patches

Every game-specific patch must be tied to the exact 3.6.1 binary by at least one strong identity mechanism:

```text
symbol + resolved address
symbol + prologue/signature guard
Ghidra/objdump evidence
hardware crash/log evidence
```

Prefer multiple guards.

Never assume a constant address delta between Android versions. Never copy another game's patch address.

Fail safely when a guard does not match; do not patch an unknown location.

## JNI and Android ABI correctness

Treat JNI signatures and guest Android C layouts as correctness-critical.

Verify:

```text
static vs instance
jclass vs jobject
argument count/types
variadic promotions
return type
jstring/jbyteArray/object representation
legacy Dalvik direct layout vs FalsoJNI-managed objects
sizeof/offsetof/alignment/packing of Android structs
```

### Bionic filesystem ABI — protected invariant

The repository previously represented Android `stat64` and `dirent64` using Vita/newlib typedefs/layouts. This was wrong and can shift guest-visible fields such as file size.

Current `source/reimpl/io.h` intentionally uses fixed-width ARM32 Bionic layouts and compile-time assertions. Do not weaken/remove those assertions or replace fields with host typedefs such as `nlink_t`, `uid_t`, `gid_t` or host `struct timespec` without proving identical layout.

`stat_newlib_to_bionic()` and `dirent_newlib_to_bionic()` must populate those guest layouts field-by-field.

## Save system — current critical investigation

Do **not** reduce Zombie Shooter's campaign persistence to "write SharedPreferences to a file".

The 3.6.1 native library exposes a richer Registry system including paths such as:

```text
RegistryPrivate::initialize/getString/setString/remove/contains/getAllKeys
RegistryPrivate::beginBatchUpdate/endBatchUpdate
RegistryPrivate::setRegSync/setRegAsync/clear
Registry::queueFlush/loadValue/storeValue
Registry::createDump/generateDump/loadDump/startDumpLoad/finishDumpLoad
```

The exact Java side also references SharedPreferences/RegistryEnumerator.

Required save workflow:

```text
exact JADX callbacks
-> exact Registry native calls
-> batching/sync/async semantics
-> file/dump format/path
-> rename/fsync/stat validation
-> close app
-> relaunch
-> verify campaign resumes instead of tutorial
```

A file being created is not enough. Save is only `GAMEPLAY VERIFIED` after successful relaunch/restoration on hardware.

Do not guess JNI array/string layouts. MetalSyntax's Zenonia work demonstrated that confusing a raw Dalvik array with FalsoJNI's actual `JavaDynArray*` can silently corrupt saves.

## Input

3.6.1 contains a real native joystick path (`AndroidJoystickControl`, `InputHandlerNative::onJoysticEvent`, motion-axis handling).

Preferred order:

```text
Vita controls
-> correct Android AInputEvent source/device/key/axis semantics
-> game's native joystick path
```

Use synthetic touch only for actions proven to be touch-driven.

Do not layer multiple translations blindly. Verify analog axes and digital buttons separately.

## Graphics and shaders

The game imports programmable GLES APIs and contains GLSL ES shader source, while important gameplay also uses a software work surface uploaded through GLES.

Keep VitaGL as compatibility baseline until evidence supports a change.

The repository already has shader cache/inventory instrumentation. Future shader work should follow:

```text
observe real glShaderSource input
-> hash/deduplicate
-> optionally dump unknown shaders in Debug
-> transpile offline with real tooling
-> psp2cgc validate
-> A/B test on real Vita
```

Do not promise gameplay FPS from shader precompilation alone; software rendering/full-frame uploads may dominate.

Useful MetalSyntax pipeline reference:

```text
GLSL ES -> glslangValidator -> SPIR-V -> SPIRV-Cross -> Cg -> psp2cgc
```

## Performance

Optimize only measured active work on real hardware.

Prefer removing work before micro-optimizing it:

```text
redundant framebuffer/texture uploads
redundant copies/conversions
repeated decode/compile work
failed path probes
unnecessary synchronization
excessive Release logging
```

Use Release builds for FPS comparisons. Debug builds are for evidence collection.

Useful telemetry concepts from `MetalSyntax/psvita-port-toolkit-cli`:

```text
perf-telemetry: frame time, p95/p99, stutters
mem-profile: live/peak heap and checkpoint survivors
logs-live: low-latency diagnostics
soak-test: long stability/hang testing
```

`sceGxmFinish()` timing is profiling-only because forcing completion changes timing.

## Audio

The recovered M4A/AAC music path was re-derived for 3.6.1 after the initial migration muted music. Preserve it unless exact evidence shows the new version requires a different path.

Do not replace working audio architecture casually. Identify the exact engine/OpenSL/player call path before changing it.

## MetalSyntax toolkit policy

The toolkit is an external engineering aid, not a runtime dependency.

High-value tools/concepts:

```text
jni-analyze
so-patch scanning
align-check
disasm
perf-telemetry
mem-profile
shader-transpile
shader-live-reload
crash/export-context tooling
```

Generated stubs/patches/shaders are candidates, not truth. Review them against the exact 3.6.1 binary.

Do not vendor the entire toolkit into this repository.

## Debugging discipline

Within a subsystem:

```text
evidence
-> small hypothesis
-> small change
-> build/regression
-> physical Vita test
-> new log/dump
-> root cause
```

Avoid unrelated speculative patch stacks.

Use bounded logging. Critical logs must flush before a likely crash; do not spam every frame in normal Release.

Useful prefixes:

```text
[BOOT] [SO] [JNI] [LIFE] [GL] [ASSET]
[AUDIO] [INPUT] [SAVE] [THREAD] [PATCH] [WARN] [CRASH]
```

For dynamic `.so` crashes:

```text
offset = PC_or_LR - logged_module_base
```

Resolve the exact offset with the matching unstripped binary/symbols/Ghidra.

## Repository / publication policy

Normal Codex work should be local unless the user explicitly authorizes remote changes. Do not push, force-update branches, publish releases or rewrite history without explicit user instruction in the active task.

The user explicitly authorized the current task to document the MetalSyntax findings and apply justified improvements on `master`; that authorization does not automatically extend to unrelated future tasks.

Do not use destructive git operations (`reset --hard`, `clean -fd`, history rewrites) as a shortcut.

## Build

Debug:

```bash
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)" 2>&1 | tee build.log
```

Release:

```bash
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)" 2>&1 | tee build.log
```

The GitHub Actions 3.6.1 workflow builds/regresses both variants and publishes physical-test prereleases after success.

## Verification vocabulary

Always distinguish:

```text
STATICALLY VERIFIED
BUILD VERIFIED
VPK VERIFIED
REAL VITA VERIFIED
GAMEPLAY VERIFIED
```

Never call something fixed merely because it compiles.

## Next engineering order

Unless newer Vita evidence changes the priority:

```text
1. verify corrected Bionic filesystem ABI builds and runs
2. reconstruct Registry/SharedPreferences end-to-end
3. verify campaign save/reload
4. verify recovered music
5. compare current 3.6.1 Release FPS to older baseline
6. profile actual bottleneck if still slower
7. finish native gamepad mapping from real event semantics
8. add optional performance/memory telemetry where useful
9. dump/transpile only real observed shaders
10. optimize/polish with A/B hardware evidence
```

## End-of-task report for Codex

Keep reports concise and include:

```text
exact target/version/hash
source evidence used
root cause(s) or remaining hypothesis
files changed
regression status
Debug build status
Release build status
VPK/release identity
verification level
exact physical-Vita test matrix
at most 1-3 requested logs/dumps
```
