# AGENTS.md — Zombie Shooter PS Vita Port

These instructions apply to the entire repository.

## Project goal

Port the Android version of **Zombie Shooter** to **real PS Vita hardware** by loading and adapting the original ARM Android shared library rather than reimplementing the whole game. The project has already reached meaningful real-Vita gameplay, so the current priority is **correctness plus measured performance**: save persistence, digital controller fidelity, renderer stability, lighting cost, general FPS and loading time.

For any Android→PS Vita porting, boot, JNI, renderer, asset, audio, input, crash, loader, save or performance task, **read `docs/METALSYNTAX_PORTING_GUIDE.md` before editing code**.

For the current hardware-tested state and the exact open blockers, **also read `docs/CURRENT_HARDWARE_BLOCKERS_2026-09-27.md` before editing code**. That document supersedes old bring-up assumptions when they conflict with current physical-Vita evidence.

## Current real-Vita baseline — 2026-09-27

Baseline tested immediately before the latest documentation pass:

`5e246c97bbf1a8de4858abb4843757dd980ed271`

Current physical-Vita facts:

- the game boots and reaches playable gameplay;
- current music/audio path works and must not be casually replaced;
- `log_mode 0`, `asset_cache_mib 16`, `software_width 864`, `framebuffer_565 0`, `software_frameskip 1` produced a subjective FPS improvement;
- a small number of textures/sprites flicker;
- dynamic lights/flashlight still cause obvious FPS drops;
- analog sticks work, but digital controls still do not match the game's Xbox layout;
- the failed synthetic/gameplay-touch experiment was removed and the previous strict/hybrid path was restored, which restored the old broken digital behavior rather than fixing it;
- the SharedPreferences/RegistryEnumerator emulation added in `5e246c97...` did not make campaign progress persist on hardware, and the intended `ux0:data/zombieshooter/shared_preferences.bin` file was not observed;
- relaunch still returns to the tutorial;
- initial loading remains a worthwhile optimization target.

Do not report any of those open items as fixed until the user verifies them on a real Vita.

The user's local workspace provides the complete original game material for reverse engineering: XAPK/APK, the canonical Android `.so`, and extracted assets/data. Locate and inspect those local files directly instead of guessing Android behavior.

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

Third-party Android-to-Vita ports may be researched for known-good patterns, especially FalsoJNI/FalsoNDK, NativeActivity and VitaGL ports. Prefer experienced public ports, including work by Rinnegatamante/MetalSyntax and other established Vita port developers, but adapt semantics rather than copying magic constants.

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

## Graphics, assets, audio, input and saves

Determine these from the game before choosing an implementation:

- **Graphics:** inspect actual GLES/EGL imports. GLES1 and GLES2 need different compatibility strategies. Treat flicker as a lifetime/state/synchronization bug until evidence proves otherwise.
- **Assets:** determine whether the engine uses `AAssetManager`, JNI resource methods, direct `fopen`, APK/ZIP/OBB, `/sdcard`, `/data/data`, or proprietary archives. Do not assume `assets/` is enough.
- **Audio:** identify OpenSL ES, AudioTrack, OpenAL, SDL, FMOD, engine mixer, Ogg/MP3/WAV, etc. before changing the current working backend.
- **Input:** prefer the engine's direct/native gamepad API when available; use synthetic touch only when that is the real control path. Real touch and synthetic touch must share safe slot allocation. Current analog success plus digital failure means axis and digital paths must be investigated separately.
- **Saves:** reconstruct the actual XAPK persistence path. Do not assume SharedPreferences is the campaign-save backend. Trace SharedPreferences, normal file I/O, app/private/external paths, registry enumeration, encryption/identity and rename/fsync behavior from the exact game version.

Keep saves, game data, logs and caches logically separated under `ux0:data/zombieshooter/`.

## Debugging policy

Use **one hardware-confirmed bug at a time inside each subsystem**, even when a pass covers several independent subsystems.

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
[AUDIO] [INPUT] [SAVE] [THREAD] [PATCH] [WARN] [CRASH]
```

When a text log cannot root-cause a crash, ask for/use the real `.psp2dmp` and analyze it against the unstripped Debug ELF. For addresses inside a dynamically loaded game `.so`, resolve the offset from the known load base manually with `nm`/`objdump`/Ghidra rather than trusting a guessed base.

## Current known baseline

The old loader-crash baseline is historical. The current real-Vita build is playable, so do not spend a new pass re-solving already working bring-up without new evidence.

Current blockers are documented in `docs/CURRENT_HARDWARE_BLOCKERS_2026-09-27.md` and are, in order of user-visible importance:

```text
save/progress persistence
correct digital Xbox-style controls
texture/sprite flicker
lighting-related FPS drop + further measured FPS improvement
initial loading time
```

For saves and input, inspect the local XAPK/DEX/smali plus the canonical `.so` before implementing another compatibility layer. For performance, profile the exact active path on real gameplay; earlier light work already demonstrated that optimizing an unused path can produce no hardware benefit.

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

- `PORTING_PLAN.md`: confirmed ABI/engine/lifecycle/assets/audio/input/save map and current hypotheses.
- `port_progress.md`: one bug entry at a time: symptom, evidence, root cause, change, verification state.
- `docs/CURRENT_HARDWARE_BLOCKERS_2026-09-27.md`: current physical-Vita blocker handoff until superseded by a newer hardware report.

Do not turn these documents into a transcript or giant speculative checklist.

## Optimization order

Meaningful gameplay has already been reached, so measured performance work is now explicitly in scope.

Prioritize optimizations that are tied to real-Vita profiling and preserve visual/gameplay correctness. Prefer removing redundant work, copies, conversions, failed path probes and synchronization waits before reducing effect quality. Overclocking, blind NEON rewrites, shader speedhacks, large caches, aggressive frame skipping, culling hacks or texture compression are not substitutes for proving the active bottleneck.

For the current pass, specifically measure and address the active light path, texture/sprite flicker interactions and startup I/O. Keep A/B flags for risky experiments and document defaults.

## End-of-task report

Keep the final Codex report concise and include:

```text
Architecture/lifecycle confirmed
Exact XAPK/.so evidence used for save and input
Most relevant external reference repository/repositories
Measured root cause(s)
Files changed
Host/ARM regression status
Debug build status
Release build status
VPK path
Verification state
Exact physical-Vita test matrix to run next
At most 1–3 logs/dumps to return
```
