# AGENTS.md — Zombie Shooter PS Vita Port

These instructions apply to the entire repository.

## Project goal

Port the Android version of **Zombie Shooter** to **real PS Vita hardware** by loading and adapting the original ARM Android shared library rather than reimplementing the whole game. The immediate priority is correctness and playability; optimization follows measured evidence.

For any Android→PS Vita porting, boot, JNI, renderer, asset, audio, input, crash, loader or performance task, read these before editing code:

```text
PORT_STATUS.md
docs/HARDWARE_TEST_LOG.md
docs/PERFORMANCE_PLAN.md
docs/METALSYNTAX_PORTING_GUIDE.md
```

`docs/HARDWARE_TEST_LOG.md` is the persistent record of real-Vita results. Do not repeat a discarded experiment or revert a hardware-confirmed fix without new evidence.

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

Also do not use GitHub CLI or any API/tool to publish or trigger remote automation:

```text
gh workflow run
gh run rerun
gh release create/edit/upload
GitHub Actions workflow_dispatch
GitHub Releases / Pre-releases creation
remote tag creation
```

The repository contains a manual GitHub Actions workflow that the **user** may run to build Debug + Release VPKs and publish a Pre-release. Codex may inspect that workflow when needed, but must not trigger it or publish anything.

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

Never reuse a CMake build directory created with a different VitaSDK ABI. If ABI/toolchain selection may have changed, delete and reconfigure that build directory.

The remote manual workflow uses the maintained `vitasdk/vitasdk-softfp:nightly` image and may have toolchain-specific compatibility shims that are not needed by the user's local GCC. Do not blindly move CI-only fixes into runtime code.

## Core porting rule: reproduce the real Android lifecycle

The lifecycle is now strongly evidenced as NativeActivity, but any major lifecycle rewrite still requires verification against the local APK/XAPK and `.so`.

Before making major boot/lifecycle changes, verify original Android behavior from:

```text
AndroidManifest.xml
JADX / smali
readelf
nm
objdump
strings
Ghidra or targeted decompilation
```

The confirmed path for the current game version is:

```text
GameActivity
→ CommonActivity
→ android.app.NativeActivity
→ metadata android.app.lib_name=zombie_shooter
→ ANativeActivity_onCreate
```

The SO does not export `JNI_OnLoad`, `android_main` or `Java_*` entrypoints.

**The decompiled Android Java/native call order remains the source of truth.**

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

Confirm static vs instance methods and exact argument/return types from Android source plus native disassembly when necessary. A function that appears to return normally can still corrupt registers/stack if its signature is wrong.

Do not replace a full FalsoJNI environment with an underspecified fake if the game dereferences a broad JNI vtable.

Do not make every missing Java method a blind no-op. Some Android calls are asynchronous and the game may require a completion/failure callback to advance.

For Android structs directly dereferenced by the `.so`, verify `sizeof`, `offsetof`, packing and alignment for the expected NDK/API definition.

## SO loading and patches

Keep loader stages explicit and observable:

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

- **Graphics:** inspect actual GLES/EGL imports. Zombie Shooter is currently bridged through VitaGL/EGL and already renders real gameplay.
- **Assets:** current NativeActivity path uses AAssetManager and game assets under `ux0:data/zombieshooter/`; preserve evidence around buffered vs streamed assets and do not speculate about the second-entry crash before reading its log/dump.
- **Audio:** the game uses OpenSL ES. The bounded `IBufferQueue_Clear` workaround is hardware-confirmed and must not be reverted casually.
- **Input:** prefer the engine's real gamepad path. Touch works. Physical Vita controls are detected as a controller but Xbox-style semantics are still pending verification/correction.

Keep saves, game data, logs and caches logically separated under `ux0:data/zombieshooter/`.

## Debugging policy

Use **one hardware-confirmed bug or one measured performance variable at a time**.

Preferred loop:

```text
evidence
→ small hypothesis
→ small change
→ build
→ physical Vita test
→ new measurement/log/core dump
→ update docs/HARDWARE_TEST_LOG.md
```

Do not stack unrelated speculative patches before a hardware test unless static analysis proves they are independently necessary.

Use a new persistent log file per run:

```text
ux0:data/zombieshooter/logs/log_<run-id>.log
```

Critical logging should flush reliably before a crash. Avoid per-frame spam.

Useful prefixes:

```text
[BOOT] [SO] [JNI] [LIFE] [GL] [ASSET]
[AUDIO] [INPUT] [THREAD] [PATCH] [PERF] [WARN] [CRASH]
```

When a text log cannot root-cause a crash, use the real `.psp2dmp` against the unstripped Debug ELF. For addresses inside the dynamically loaded game `.so`, resolve offsets from the known load base manually with `nm`/`objdump`/Ghidra rather than guessing.

## Current hardware-confirmed baseline

Current real-Vita state as of 2026-09-25:

```text
SO loads and relocates
412 undefined imports have explicit static coverage
41/41 init_array constructors return
NativeActivity bootstrap reaches the game
graphics render logos / LOADING / tutorial / menu
touch works
OpenSL ES audio plays
bounded IBufferQueue_Clear fix is hardware-confirmed
user can walk through tutorial and return to main menu
```

Known unresolved issues:

```text
very low performance
second tutorial entry: missing images/textures then crash
physical controller mapping not yet correct
Play Asset Delivery/commonAssets incomplete
Wrong AES key length still observed historically
```

Performance baseline from the Release MSAA-NONE test:

```text
startup ≈ 7 FPS
LOADING ≈ 4 FPS
LOADING ≈ 5 minutes
```

Important result already learned:

```text
MSAA 4x → NONE did NOT produce a meaningful performance improvement.
```

The next controlled performance variable is Release logging overhead. The current code prepares a quiet Release while keeping Debug verbose. See `docs/HARDWARE_TEST_LOG.md` and `docs/PERFORMANCE_PLAN.md` before proposing another graphics speedhack.

The second-entry tutorial crash is a separate track. The user has a real log and `.psp2dmp` on their PC; inspect those before changing assets/lifecycle/memory for that bug.

## Build commands

Debug diagnostic build:

```bash
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"
cd ~/Zombie-Shooter-PS-Vita-Port
rm -rf build-session-debug
cmake -S . -B build-session-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-session-debug -j"$(nproc)" 2>&1 | tee build-session-debug.log
```

Release gameplay/performance build:

```bash
rm -rf build-session-release
cmake -S . -B build-session-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-session-release -j"$(nproc)" 2>&1 | tee build-session-release.log
```

Verify actual outputs before claiming success:

```bash
find build-session-debug build-session-release \
  \( -iname '*.vpk' -o -iname 'eboot.bin' \) -type f
```

Expected VPKs:

```text
build-session-debug/zombie_shooter.vpk
build-session-release/zombie_shooter.vpk
```

Do not invent or add a third `Perf` variant unless the user explicitly decides that Debug + Release are insufficient. The current intended workflow is exactly two VPKs.

## Verification vocabulary

Distinguish explicitly:

```text
STATICALLY VERIFIED
BUILD VERIFIED
VPK VERIFIED
PENDING HARDWARE
REAL VITA VERIFIED
GAMEPLAY VERIFIED
NO MEANINGFUL IMPROVEMENT
REGRESSION
```

Never describe something as fixed merely because it compiles.

## Documentation while porting

Maintain these project notes:

- `PORTING_PLAN.md`: confirmed ABI/engine/lifecycle/assets/audio/input map and current hypotheses.
- `PORT_STATUS.md`: concise current snapshot and priorities.
- `port_progress.md`: technical bug-by-bug history accumulated during bring-up.
- `docs/HARDWARE_TEST_LOG.md`: **mandatory chronological record of real-Vita tests and A/B results**.
- `docs/PERFORMANCE_PLAN.md`: current evidence-driven optimization order.

After any meaningful physical-Vita result, update `docs/HARDWARE_TEST_LOG.md` before starting a new unrelated hypothesis. Record:

```text
build / commit / tag
isolated change
measurement before / after
real hardware result
verification state
supported conclusion
next controlled test
```

Do not turn documents into raw transcripts. Record decisions and evidence that future sessions need.

## Optimization order

Use the current plan, not generic optimization advice.

As of 2026-09-25:

```text
1. hardware-test the quiet Release
2. if still slow, profile asset I/O + waits/sleeps/synchronization
3. compare CPU vs GPU clocks without changing code
4. profile CPU/frame work
5. inspect VitaGL memory/pools
6. shader cache if relevant
7. VitaGL speedhacks one at a time
8. internal resolution only if evidence shows GPU-bound
```

Do not repeat MSAA as the next experiment; it has already been tested without meaningful improvement.

Do not prioritize large groups of speedhacks, frame skipping, culling hacks or texture changes before the current bottleneck is measured.

## End-of-task report

Keep the final Codex report concise and include:

```text
Architecture/lifecycle confirmed
Most relevant external reference(s)
Files changed
Debug build status
Release build status
VPK paths
Verification state
Documentation updated
Exact physical-Vita test to run next
At most 1–3 logs/dumps to return
```
