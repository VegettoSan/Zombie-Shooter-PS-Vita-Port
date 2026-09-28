# MetalSyntax toolkit findings for Zombie Shooter 3.6.1

Date: 2026-09-28

This document is a permanent engineering handoff for humans, ChatGPT and Codex. It records the parts of MetalSyntax's Android -> PS Vita methodology/toolkit that are directly relevant to this port, the concrete bugs found while comparing those references with this repository, and the rules for applying the ideas safely.

## Current target

The active Android target is:

```text
Zombie Shooter 3.6.1
versionCode 1161
package com.sigmateam.zombieshooter.free
ABI armeabi-v7a / ARM32 EABI5 soft-float
native library libzombie_shooter.so
SHA-256 cb461ac47de79536824e8f7b64fa82293b796bcc159675b4f5ad6304e60bdca1
```

The game uses `NativeActivity`, FalsoNDK/FalsoJNI compatibility, EGL/GLES through VitaGL, OpenSL ES/audio replacement work, and a native game engine with symbols still present in the ARMv7 library.

Never reuse 3.2.3/3.5.3 offsets in 3.6.1 without independently resolving and validating the exact symbol/prologue.

## Primary external references

### Toolkit

- `MetalSyntax/psvita-port-toolkit-cli`

Important modules researched:

```text
psvita_toolkit/catalog.py
psvita_toolkit/jni_analyzer.py
psvita_toolkit/so_patcher.py
psvita_toolkit/perf_telemetry.py
psvita_toolkit/mem_profiler.py
psvita_toolkit/shader_transpiler.py
psvita_toolkit/asset_transcoder.py
```

### Mature/reference ports

Most useful save/performance examples found:

```text
MetalSyntax/Zenonia4-psvita-port
MetalSyntax/ILLUSIA-2-Vita
MetalSyntax/Advena-Vita
```

Do not vendor those projects or copy binary addresses. Reuse methodology and ABI-correct compatibility patterns only.

---

## 1. Critical ABI finding: Android `stat/stat64`

### Problem found in this repository

Before the 2026-09-28 correction, `source/reimpl/io.h` declared guest Android `stat64_bionic` using Vita/newlib typedefs such as:

```c
nlink_t
uid_t
gid_t
struct timespec
```

That is ABI-unsafe. Vita/newlib and Android/Bionic do not give those typedefs the same sizes on ARM32. In particular Vita/newlib's `nlink_t` is 16-bit while Android 7/Bionic uses a 32-bit `nlink_t`; Android's uid/gid are also 32-bit.

A wrong field width shifts all fields that follow it, including `st_size`. The engine can therefore write a valid file and then see the wrong size when it validates the save through `stat()` or `fstat()`.

MetalSyntax documented essentially this failure mode in Illusia 2: the save existed but its Bionic-facing stat layout was wrong, causing save/load validation to fail.

### Required ARM32 Bionic layout

The port now defines the guest structure with fixed-width types and explicit padding. Required checkpoints:

```text
sizeof(stat64_bionic)       = 104 / 0x68
offset st_nlink             = 0x14
offset st_size              = 0x30
offset st_blocks            = 0x40
offset st_atim              = 0x48
offset final st_ino         = 0x60
```

The header contains compile-time assertions. Do not remove them.

`stat_newlib_to_bionic()` must clear the complete destination first so all ABI padding is deterministic, then copy values field-by-field with explicit fixed-width casts.

### Why this matters to Zombie Shooter

`libzombie_shooter.so` imports normal `stat()`/`fstat()` and the soloader resolves them to `stat_soloader()`/`fstat_soloader()`. This is therefore guest-visible ABI, not an internal implementation detail.

Verification state after source change:

```text
STATICALLY VERIFIED
BUILD/REAL VITA verification still required
```

---

## 2. Critical ABI finding: Android `dirent/dirent64`

The inherited wrapper also had a wrong Android directory entry layout. It used a 16-bit inode and a 64-bit record length.

Android/Bionic 32-bit public `dirent` and `dirent64` use:

```c
uint64_t d_ino;
int64_t  d_off;
uint16_t d_reclen;
uint8_t  d_type;
char     d_name[256];
```

with natural total size 280 bytes.

The port now uses the fixed layout plus compile-time size/offset assertions. The converter populates `d_ino`, sets `d_reclen` to the guest record size, preserves `d_type`, zeroes padding, and safely terminates `d_name`.

This may affect directory enumeration and asset/file discovery even when it is not the root cause of campaign save persistence.

---

## 3. Save system: reproduce the engine contract, not a generic preference file

### What 3.6.1 actually exposes

The 3.6.1 native library contains a complete Registry system, including symbols such as:

```text
core::RegistryPrivate::initialize
core::RegistryPrivate::getString
core::RegistryPrivate::setString
core::RegistryPrivate::remove
core::RegistryPrivate::contains
core::RegistryPrivate::getAllKeys
core::RegistryPrivate::beginBatchUpdate
core::RegistryPrivate::endBatchUpdate
core::RegistryPrivate::setRegSync
core::RegistryPrivate::setRegAsync
core::RegistryPrivate::clear

core::Registry::queueFlush
core::Registry::loadValue
core::Registry::storeValue
core::Registry::createDump
core::Registry::generateDump
core::Registry::loadDump
core::Registry::startDumpLoad
core::Registry::finishDumpLoad
```

The binary also contains Java/JNI strings for `SharedPreferences`, `getPreferences`, `RegistryEnumerator`, and save/dump paths.

Therefore campaign persistence must not be treated as merely `putString()` -> one arbitrary Vita file. The exact Java/native transaction model has to be reconstructed.

### Required investigation order

1. Decompile the exact 3.6.1 DEX with JADX.
2. Inspect `com.sigmateam.sige.RegistryEnumerator` and every Activity/SharedPreferences helper it calls.
3. Disassemble/decompile the exported `RegistryPrivate::*` functions from the exact 3.6.1 SO.
4. Trace:
   - preference object acquisition;
   - key enumeration;
   - get/set/remove;
   - batch begin/end;
   - synchronous vs asynchronous flush;
   - registry dump generation/load;
   - any CRC/encryption/identity requirements;
   - `stat/fstat/access/rename/fsync` validation.
5. Implement only the Java/JNI and file semantics proven necessary.
6. Hardware test: finish tutorial -> force a save -> verify files -> close app -> relaunch -> verify progress.

### Lesson from Zenonia 4

MetalSyntax found a real save corruption bug caused by confusing two byte-array representations: an engine-specific raw Dalvik layout used for assets versus FalsoJNI's actual `JavaDynArray*` returned by `NewByteArray`. The wrong interpretation produced an absurd save length and wrote adjacent memory.

Rule for this repository:

> Never assume a JNI object representation from another code path. Confirm whether the exact caller uses FalsoJNI arrays/functions or directly dereferences a legacy Dalvik-like structure.

### Save verification requirements

A save is not fixed because a file appears. Verify all of:

```text
file created
size reported correctly through guest stat/fstat
content survives app exit
engine accepts content on next launch
campaign resumes beyond tutorial
```

Only the last item is `GAMEPLAY VERIFIED`.

---

## 4. Performance methodology to adopt

MetalSyntax's toolkit includes `perf-telemetry`, `mem-profile`, `logs-live`, `soak-test` and crash-analysis helpers. We already have substantial local instrumentation, so do not replace it blindly. Reuse the measurement model.

### Frame telemetry

Measure on physical Vita:

```text
FPS
average frame time
p50/p95/p99 frame time
stutters > 2x average
VitaGL swap/present time
software renderer phase time
full-frame texture upload time
shader compile/link time
asset I/O during loading
```

A profiling-only `sceGxmFinish()` timing can be useful as a real GPU completion signal, but it forces synchronization and must not be enabled in play/release baselines.

### Memory telemetry

The toolkit's memory profiler generates allocation wrappers and streams alloc/free/checkpoint events over UDP. Our loader already owns the import table for `malloc/calloc/realloc/free`, making the concept directly applicable.

Use this for diagnostic builds to answer:

```text
live heap bytes
peak heap bytes
allocations surviving level transitions
cache growth
possible leaks
```

Do not ship per-allocation UDP tracing in Release.

### Optimization rule learned from mature ports

Do not assume recompiling the same CPU algorithm with newer ARM/NEON code will move FPS. MetalSyntax repeatedly found memory-bandwidth/work-volume bottlenecks where codegen changes produced little or no gain.

Prefer eliminating:

```text
redundant full-frame uploads
redundant copies/conversions
repeated decode work
failed path probes
unnecessary synchronization
unnecessary shader compilation
```

before visual degradation or unsafe speedhacks.

---

## 5. Zombie Shooter renderer/shader findings

The 3.6.1 library imports the programmable GLES shader API, including:

```text
glCreateShader
glShaderSource
glCompileShader
glCreateProgram
glAttachShader
glLinkProgram
glUseProgram
glGetUniformLocation
glUniform*
glDrawArrays
glDrawElements
glTexImage2D
glTexSubImage2D
```

The library contains GLSL ES shader source strings. So shader tooling is relevant even though a substantial gameplay path uses a software work surface uploaded to a texture.

### MetalSyntax shader-transpile pipeline

The toolkit uses a semantic toolchain rather than regex-only rewriting:

```text
GLSL ES 1.00
-> mechanical ES 3.10 compatibility preprocessing
-> glslangValidator
-> SPIR-V
-> SPIRV-Cross HLSL Shader Model 3
-> narrow HLSL-to-Cg cleanup
-> psp2cgc validation
```

It explicitly removes SPIRV-Cross's D3D9 half-pixel correction because it is wrong for Vita/GXM.

### Recommended Zombie Shooter shader workflow

Do not replace VitaGL's shader path immediately. First create an observational dump/cache layer:

```text
glShaderSource
-> normalize/deduplicate source
-> hash shader + stage
-> write unknown GLSL source once in Debug
-> compile normally through VitaGL
```

Then offline-transpile only shaders actually observed on hardware. Validate generated Cg/GXP before experimenting with direct cache reuse.

Expected benefits are primarily:

```text
faster/consistent shader warmup
fewer shader-related loading stalls
better compatibility/debugging
ability to tune a known expensive shader
```

Do not promise a large gameplay FPS increase merely from precompiling shaders; the current software framebuffer path may dominate gameplay cost.

---

## 6. Asset transcoding: later-stage optimization

The toolkit can create pre-mipped raw texture containers and optionally hardware-compressed assets if external tools such as PVRTexToolCLI are present. It also has batch audio conversion helpers.

Do not globally transcode Zombie Shooter assets yet. The engine owns proprietary resource/texture semantics and mixes software rendering with GLES. First prove which decoded textures are expensive and who consumes them.

A safe future candidate is an opt-in cache for known immutable decoded assets, with memory telemetry and fallback to the original path.

---

## 7. `jni-analyze`, `so-patch`, `disasm`, `align-check`

### `jni-analyze`

Useful for generating a checklist/scaffold of Java callbacks from the exact JADX output. Generated bodies are not authoritative; verify every signature and return semantic.

### `so-patch`

Useful to detect Android-only telemetry/IAP SDKs and hardcoded `/data/data` or `/sdcard` paths. It can mechanically apply a confirmed ARM/Thumb safe-return patch with backup/audit metadata.

Never let it choose an address by guesswork. Address confirmation must come from the exact 3.6.1 binary via symbol/disassembly/Ghidra/hardware evidence.

### `disasm`

High-value for this game because the 3.6.1 library retains many useful C++ symbols. Prefer symbol-driven investigation over magic offsets.

### `align-check`

Worth running against the game library and compatibility code for ARM instructions/loads that assume Android alignment behavior and may fault on Vita's Cortex-A9.

---

## 8. Integration policy for this repository

Do not vendor the entire MetalSyntax toolkit into the port.

Use it as an external engineering tool or adapt small, clearly licensed, game-agnostic ideas when they provide a concrete benefit. Any adapted implementation must be understandable without the toolkit being installed.

Every risky feature should be behind a build/config flag until real Vita evidence justifies making it default.

Never:

```text
copy another game's offsets
copy a shader blindly
copy save handlers without verifying the JNI object type
claim FPS from host/Vita3K
claim save fixed because a file was created
ship profiling stalls/log spam in Release
```

---

## 9. Current prioritized engineering queue

1. Keep the 3.6.1 recovery build as the current hardware checkpoint until tested.
2. Validate the corrected Bionic `stat/stat64` and `dirent` ABI in CI/build.
3. Reconstruct 3.6.1 Registry/SharedPreferences semantics end-to-end.
4. Verify campaign save/load on physical Vita.
5. Compare 3.6.1 FPS against the known older baseline using Release only.
6. If still slower, profile the actual active renderer phases before adding more speed patches.
7. Add optional UDP performance telemetry/dashboard compatibility.
8. Add Debug-only shader-source dumping/deduplication.
9. Transpile only real observed GLSL sources and validate with `psp2cgc`.
10. Consider memory profiler hooks before introducing large caches.

---

## 10. Verification vocabulary

Always use these states precisely:

```text
STATICALLY VERIFIED
BUILD VERIFIED
VPK VERIFIED
REAL VITA VERIFIED
GAMEPLAY VERIFIED
```

As of creation of this document, the Bionic struct corrections are only STATICALLY VERIFIED. The current 3.6.1 recovery VPK #28 was built successfully but its recovered music/FPS/controls/save behavior still requires a new physical Vita test.
