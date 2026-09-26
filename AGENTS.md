# AGENTS.md — Zombie Shooter PS Vita Port

These instructions apply to the entire repository.

## Project goal

Port the Android version of **Zombie Shooter** to **real PS Vita hardware** by loading and adapting the original ARM Android shared library rather than reimplementing the whole game. The immediate priority is correctness and playability; optimization comes later.

For any Android→PS Vita porting, boot, JNI, renderer, asset, audio, input, crash, or loader task, **read `docs/METALSYNTAX_PORTING_GUIDE.md` before editing code**.

## Explicit restoration exception (2026-09-25)

The user explicitly authorized this one restoration task to fetch, create a remote
backup, commit, push with an exact SHA lease, and run GitHub Actions. The functional
local working tree at `23d92dc` plus its uncommitted changes is the source of truth.
The former remote `2809a6951cec83a460be5777f388edf04a76f259` is preserved at
`backup/before-local-restore-20260925`. No pull, merge or rebase is permitted.
This exception does not authorize future publication: normal work remains local-only.

`demo/libzombie_shooter.so` is the immutable canonical crash-analysis reference.
Never delete, rename, move, strip or patch it. SHA-256:
`5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7`.

## Work only on the local checkout

The working directory is expected to be:

```bash
cd ~/Zombie-Shooter-PS-Vita-Port
```

Modify the local project freely, but do **not** publish or alter remote Git state.

Do not run in this repository:

```bash
git push
git pull
git fetch
git commit
git merge
git rebase
git cherry-pick
git reset --hard
git clean -fd
```

Git may be used for inspection only:

```bash
git status --short
git diff
git diff --stat
git log --oneline
git branch --show-current
```

Public external repositories may be cloned into `/tmp/...` for research when useful. Never mix those clones into this repository unless code is intentionally adapted and its license/provenance is understood.

## Target hardware

The target is **PS Vita real hardware**.

Do not spend task time installing, configuring, launching, or debugging with Vita3K. When runtime verification is required, produce a useful VPK/logging build for the user to test on a physical Vita.

## VitaSDK selection

Zombie Shooter must use the SoftFP SDK:

```text
/usr/local/vitasdk
```

Initialize it before configuring CMake:

```bash
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"
```

Verify:

```bash
"$VITASDK/bin/arm-vita-eabi-gcc" -Q --help=target | grep float
```

It must report `-mfloat-abi=softfp`.

The user's normal HardFP SDK for other Vita projects is preserved at:

```text
/usr/local/vitasdk-hardfp
```

**Never modify or replace `/usr/local/vitasdk-hardfp`.**

Never reuse a CMake build directory created with a different VitaSDK ABI. If ABI/toolchain selection may have changed, delete and reconfigure `build/`.

## Core porting rule: reproduce the real Android lifecycle

Do not assume the current loader architecture is correct just because it reaches part of the game.

Before making major boot/lifecycle changes, verify the original Android behavior from the local APK/XAPK and `.so` using some combination of:

```text
AndroidManifest.xml
JADX / smali
readelf
nm
objdump
strings
Ghidra or targeted decompilation
```

Determine whether the game actually uses:

```text
Java_* exported JNI methods
JNI_OnLoad
RegisterNatives
ANativeActivity_onCreate / NativeActivity
android_main
GLSurfaceView / Renderer callbacks
another engine-specific lifecycle
```

Then reproduce that lifecycle in the Vita loader in the same semantic order.

**The decompiled Android Java/native call order is the source of truth.**

If the current `NDK_PORT`/NativeActivity path is wrong, keep working loader pieces that are valid and replace the bootstrap instead of patching fake callbacks indefinitely.

## MetalSyntax methodology

Follow the evidence-driven workflow documented in `docs/METALSYNTAX_PORTING_GUIDE.md`:

```text
analyze APK + .so
→ identify ABI, GLES, engine and real lifecycle
→ compare only with genuinely similar ports
→ implement the minimum Android/JNI surface the game actually needs
→ build
→ test on real Vita
→ inspect log / .psp2dmp
→ root-cause one confirmed bug
→ repeat
```

Reuse architecture from another port only after establishing a real engine/ABI/lifecycle match. Never copy binary patch offsets between different `.so` files or versions.

## JNI / ABI correctness

Treat JNI signatures and ARM ABI as correctness-critical.

Confirm static vs instance methods and the exact argument/return types from Android source plus native disassembly when necessary. A function that appears to return normally can still corrupt registers/stack if its signature is wrong.

Do not replace a full FalsoJNI environment with an underspecified fake if the game dereferences a broad JNI vtable.

Do not make every missing Java method a blind no-op. Some Android calls are asynchronous and the game may require a completion/failure callback to advance.

For Android structs directly dereferenced by the `.so`, verify `sizeof`, `offsetof`, packing and alignment for the expected NDK/API definition.

## SO loading and patches

Keep the loader stages explicit and observable:

```text
load
relocate
resolve imports
apply justified compatibility patches
flush/invalidate caches as required
run constructors / init array
enter the proven game lifecycle
```

For multiple native modules, derive dependencies from the actual binary (`DT_NEEDED`, Java loader behavior, etc.) and use non-overlapping load bases.

Never apply an arbitrary runtime/binary patch merely because another game needed one. A patch must be tied to evidence from this game's binary, log, disassembly, or crash dump.

## Graphics, assets, audio and input

Determine these from the game before choosing an implementation:

- **Graphics:** inspect actual GLES/EGL imports. GLES1 and GLES2 need different compatibility strategies.
- **Assets:** determine whether the engine uses `AAssetManager`, JNI resource methods, direct `fopen`, APK/ZIP/OBB, `/sdcard`, `/data/data`, or proprietary archives. Do not assume `assets/` is enough.
- **Audio:** identify OpenSL ES, AudioTrack, OpenAL, SDL, FMOD, engine mixer, Ogg/MP3/WAV, etc. before building a Vita backend.
- **Input:** prefer the engine's direct/native gamepad API when available; use synthetic touch only when that is the real control path. Real touch and synthetic touch must share safe slot allocation.

Keep saves, game data, logs and caches logically separated under `ux0:data/zombieshooter/`.

## Debugging policy

Use **one hardware-confirmed bug at a time**.

Preferred loop:

```text
evidence
→ small hypothesis
→ small change
→ build
→ physical Vita test
→ new log/core dump
```

Do not stack unrelated speculative patches before a hardware test unless static analysis proves they are independently necessary.

Use a new persistent log file per run, e.g.:

```text
ux0:data/zombieshooter/logs/log_<run-id>.log
```

Critical logging should flush reliably before a crash. Avoid per-frame spam.

Useful prefixes:

```text
[BOOT] [SO] [JNI] [LIFE] [GL] [ASSET]
[AUDIO] [INPUT] [THREAD] [PATCH] [WARN] [CRASH]
```

When a text log cannot root-cause a crash, ask for/use the real `.psp2dmp` and analyze it against the unstripped Debug ELF. For addresses inside a dynamically loaded game `.so`, resolve the offset from the known load base manually with `nm`/`objdump`/Ghidra rather than trusting a guessed base.

## Current known baseline

Previous real-Vita testing reached approximately:

```text
SO loaded
SO relocated
imports resolved
41/41 init_array constructors returned
OpenGL preload returned
FalsoJNI initialized
gl_init returned
game/NDK thread created
ANativeActivity_onCreate found and returned
sigaction(SIGSEGV) warnings
crash
```

This is a useful clue, not proof that NativeActivity is the correct lifecycle. Revalidate against the local APK/XAPK before spending major effort on NativeActivity internals.

## Build commands

Debug bring-up:

```bash
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"
cd ~/Zombie-Shooter-PS-Vita-Port
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)" 2>&1 | tee build.log
```

Release checkpoint:

```bash
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)" 2>&1 | tee build.log
```

Verify actual outputs before claiming success:

```bash
find build \( -iname '*.vpk' -o -iname 'eboot.bin' \) -type f
```

## Verification vocabulary

Distinguish these explicitly:

```text
STATICALLY VERIFIED
BUILD VERIFIED
VPK VERIFIED
REAL VITA VERIFIED
GAMEPLAY VERIFIED
```

Never describe something as fixed merely because it compiles.

## Documentation while porting

Keep or create concise project notes:

- `PORTING_PLAN.md`: confirmed ABI/engine/lifecycle/assets/audio/input map and current hypotheses.
- `port_progress.md`: one bug entry at a time: symptom, evidence, root cause, change, verification state.

Do not turn either document into a transcript or giant speculative checklist.

## Optimization order

Do not prioritize overclocking, NEON tuning, shader speedhacks, large caches, FBO downsampling, frame skipping, culling hacks or texture compression until the game reaches meaningful gameplay and the bottleneck has been measured.

Correctness and reproducibility first; performance second.

## End-of-task report

Keep the final Codex report concise and include:

```text
Architecture/lifecycle confirmed
Most relevant external reference(s)
Files changed
Debug build status
Release build status
VPK path
Verification state
Exact physical-Vita test to run next
At most 1–3 logs/dumps to return
```
