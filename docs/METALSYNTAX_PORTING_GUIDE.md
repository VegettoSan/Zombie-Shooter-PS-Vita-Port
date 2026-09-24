# MetalSyntax Android → PS Vita Porting Guide

This document summarizes the recurring engineering method used across MetalSyntax Android→PS Vita ports and translates it into rules for Zombie Shooter.

It is a methodology reference, not a source of offsets or game-specific patches.

## Primary references

Useful MetalSyntax repositories include:

```text
MetalSyntax/Asphalt-5-Vita
MetalSyntax/Asphalt-6-Vita
MetalSyntax/Shadow-Guardian-vita
MetalSyntax/Dungeon-Hunter-2-vita
MetalSyntax/Sacred-Odyssey-vita
MetalSyntax/Gangstar-Miami-Vindication-Vita
MetalSyntax/Zenonia2-psvita-port
MetalSyntax/zenonia3-psvita-port
MetalSyntax/Zenonia4-psvita-port
MetalSyntax/Inotia3-vita
MetalSyntax/Advena-Vita
MetalSyntax/ILLUSIA-vita
MetalSyntax/ILLUSIA-2-Vita
MetalSyntax/Inmortal-Dusk-vita
MetalSyntax/The-Impossible-Game-vita
MetalSyntax/prince-of-persia-classic-psvita-port
MetalSyntax/psvita-port-toolkit-cli
```

Research them selectively. First classify Zombie Shooter, then inspect only the ports that match its ABI, engine style, JNI model and renderer.

External public repositories may be cloned under something like:

```bash
mkdir -p /tmp/zombie-metalsyntax-research
```

Do not vendor entire reference ports into this project.

---

## 1. Recurring MetalSyntax workflow

Across the mature ports, the productive pattern is:

```text
APK/XAPK inspection
→ native library inspection
→ Java + native lifecycle reconstruction
→ minimal Vita-side Android/JNI bridge
→ build
→ physical Vita test
→ incremental file log
→ crash dump if needed
→ disassembly/decompilation of the exact failing path
→ one evidence-backed fix
→ repeat
```

The important distinction is that the loader is built *around the original game's behavior*. It is not a generic Android emulator.

---

## 2. Asphalt 5: exact Java lifecycle matters

Asphalt 5 is a strong example of why exported JNI names alone are not enough.

The port ultimately reproduced the Android renderer's real initialization order rather than merely calling plausible native functions. Its bring-up sequence included the equivalent of:

```text
initialize Vita graphics
→ nativeGetJNIEnv
→ GLResLoader native init
→ GLMediaPlayer native init
→ game native init
→ renderer native init
→ renderer native render loop
```

A key finding was that `nativeGetJNIEnv` primed a native global `JNIEnv*` that later entrypoints internally dereferenced. Skipping that function caused a crash even though later functions themselves accepted a `JNIEnv*` argument.

### Rule for Zombie Shooter

Use the decompiled Java/renderer order as the source of truth. A method that appears trivial can establish hidden native state required later.

Do not infer initialization order only from export names.

---

## 3. Zenonia 2/3: reuse proven engine work, not binary offsets

Zenonia 3 deliberately reused a Zenonia 2 base only after static analysis showed both games used the same Gamevil Nexus2/Clet family and shared JNI patterns.

Even then, the port rechecked real differences:

```text
exported method set
function signatures
argument counts
init order
input API
framebuffer behavior
asset behavior
```

A major lesson is that engine logic can be reusable while binary addresses are not.

### Rule for Zombie Shooter

If another Android game/port shares Zombie Shooter's actual engine:

- reuse compatible loader architecture;
- reuse generic JNI helpers/wrappers when ABI semantics match;
- rederive every game-version-specific offset;
- verify every function signature independently.

Never copy another game's runtime patch address.

---

## 4. Shadow Guardian: manual JNI lifecycle around a proprietary 3D engine

Shadow Guardian is useful when a game has a Java Activity/renderer around a large GLES2 native engine.

The port reconstructs a concrete startup chain through native methods and then drives a persistent native render loop. It also replaces Android platform surfaces with Vita implementations only where required:

```text
vitaGL for GLES
sceAudioOut for engine audio output
path redirection to ux0:
Vita input → game's real touch/key entrypoints
platform/license compatibility patches
```

### Rule for Zombie Shooter

If Zombie Shooter's APK reveals a Java Activity or renderer calling explicit native methods, prefer reproducing that sequence directly over simulating a whole NativeActivity stack.

---

## 5. Asphalt 6 / Sacred Odyssey / Dungeon Hunter 2 / Gangstar: evidence-driven 3D debugging

These ports show how the workflow scales when the engine is much more complex.

Recurring techniques include:

```text
on-device logs
watchdog/heartbeat instrumentation
breadcrumb ring buffers
crash dumps
manual PC/LR symbolization
Ghidra/objdump around exact failing functions
runtime patches tied to proven engine bugs
engine-specific audio bridges
careful asset/path redirection
```

The important rule is not to patch a symptom simply because it is near the crash in the log.

A real fix normally has a chain such as:

```text
console symptom
→ log/dump evidence
→ exact native function
→ Java/native cross-reference
→ root cause
→ small compatibility fix
```

### Rule for Zombie Shooter

When the game reaches increasingly deep code, stop widening the Android emulation layer speculatively. Instrument the exact failing boundary instead.

---

## 6. The Impossible Game: minimum environment wins

The Impossible Game demonstrates the opposite extreme.

Its native library needed very little Android/JNI behavior, so the port did not build a giant fake Android runtime. The native gameplay library was driven with a small set of entrypoints while Java-side UI/audio/save behavior was replaced natively where that was simpler and more reliable.

### Rule for Zombie Shooter

After inspecting imports and call paths, implement only the Android facilities Zombie Shooter actually touches.

A smaller bridge is usually easier to debug than a broad collection of incomplete stubs.

---

## 7. Prince of Persia Classic: multi-library and complex asset layouts

Prince of Persia is useful when an Android game has:

```text
multiple native libraries
Cocos2d-x or another middleware layer
APK/OBB resources
loose-file fallbacks
video/audio subsystems
```

The important patterns are:

- determine each native module and its load order;
- use separate non-overlapping load addresses;
- preserve the game engine's expected data layout;
- redirect file access coherently rather than patching dozens of unrelated paths;
- keep a Debug build with useful logging and a Play/Release build without heavy diagnostics.

### Rule for Zombie Shooter

If XAPK inspection reveals more required `.so` modules or split data, do not assume `libzombie_shooter.so` is the entire native environment.

---

## 8. psvita-port-toolkit: what the consolidated workflow automates

MetalSyntax's `psvita-port-toolkit-cli` consolidates techniques from multiple real ports.

Useful conceptual steps from it include:

```text
APK identification/extraction
ABI and GLES detection
JADX Java decompilation
Ghidra/native decompilation
symbol search
build/deploy
real-console logging
crash dump download
vita-parse-core analysis
ARM alignment checks
shader inspection
asset verification
```

Zombie Shooter does not need to adopt the entire toolkit to benefit from the workflow.

Use individual tools when they shorten the evidence path.

---

## 9. Required initial classification for Zombie Shooter

Before major loader edits, determine these from the local APK/XAPK and `.so`.

### Android side

```text
package name
main Activity
NativeActivity yes/no
minSdk / targetSdk
screen orientation
uses-feature GLES
System.loadLibrary calls
native method declarations
GLSurfaceView/renderer classes
onCreate/onStart/onResume/onPause/onDestroy
onSurfaceCreated/onSurfaceChanged/onDrawFrame
input handlers
asset access
sound initialization
```

### Native side

Use targeted inspection such as:

```bash
file libzombie_shooter.so
readelf -h libzombie_shooter.so
readelf -A libzombie_shooter.so
readelf -d libzombie_shooter.so
readelf -Ws libzombie_shooter.so
nm -D -C libzombie_shooter.so
objdump -T libzombie_shooter.so
strings -a libzombie_shooter.so
```

Classify:

```text
ARMv6 / ARMv7
softfp / hardfp
Thumb/ARM use
NEON expectations
DT_NEEDED
Java_* exports
JNI_OnLoad
RegisterNatives
ANativeActivity_onCreate
android_main
GLES1 / GLES2
EGL usage
OpenSL ES / AudioTrack / other audio
thread APIs
asset APIs
network/ads/IAP/crash SDKs
```

---

## 10. NativeActivity decision gate

The current Zombie Shooter loader has previously reached `ANativeActivity_onCreate` and received a successful return.

That alone does **not** prove NativeActivity is the correct launch model.

Before spending substantial effort on `ANativeWindow`, `AInputQueue`, `ALooper` or callback emulation, answer:

```text
Does the original APK actually launch the game through NativeActivity semantics?
```

### If YES

Then validate the Vita-side definitions and lifecycle for:

```text
ANativeActivity
ANativeActivityCallbacks
ANativeWindow
AInputQueue
ALooper
AAssetManager
```

Check actual struct size/offset/alignment against the NDK definition expected by the game.

Instrument each lifecycle callback before and after invocation.

### If NO

Stop deepening the fake NativeActivity implementation.

Keep reusable loader pieces and replace the bootstrap with the real Java/JNI lifecycle discovered from the APK.

---

## 11. Current crash clue: `sigaction(SIGSEGV)`

Previous hardware logging showed calls resembling:

```text
ANativeActivity_onCreate returned
sigaction(11, ...): not implemented
sigaction(11, ...): not implemented
crash
```

Treat this as a strong clue but not proof of causality.

After lifecycle validation, instrument the signal wrapper to identify:

```text
signal number
new handler
old handler request
flags
caller/LR
caller offset inside libzombie_shooter.so
handler offset if inside the SO
```

Then determine whether the caller belongs to:

```text
engine exception handling
crash reporting SDK
telemetry library
C++ runtime
another subsystem
```

If it is merely a crash reporter, fully emulating POSIX signal behavior may be unnecessary. If engine logic intentionally uses signal recovery, it may be critical.

---

## 12. JNI rules

A JNI bug can survive several calls before finally crashing.

Always verify:

```text
static vs instance
jclass vs jobject
argument count
jint/jboolean/jlong/jfloat/jdouble
return type
array/string object representation
```

When old Android binaries access Dalvik-era objects directly rather than through JNI APIs, inspect the exact expected native layout rather than returning a modern-looking fake object.

Do not assume `0`, `NULL`, or `JNI_TRUE` is a safe generic stub result.

---

## 13. Graphics rules

### GLES1 / fixed function

Potential issues seen across ports include:

```text
GL_FIXED representation
RGB565 uploads
texture filtering/completeness
client vertex arrays
fixed-function state assumptions
```

### GLES2 / programmable pipeline

Potential issues include:

```text
GLSL accepted on Android but rejected by Vita shader compiler
precision syntax
unsupported extensions
FBO/render-target differences
shader caching
attribute/uniform limits
```

Do not enable aggressive vitaGL speedhacks until the renderer works correctly.

---

## 14. Asset rules

Do not assume the APK's `assets/` folder maps directly to Vita.

The game may access resources through:

```text
AAssetManager
Java resource helper methods
direct file paths
APK ZIP
OBB
/sdcard
/data/data
proprietary packed archives
resource IDs
```

Reconstruct the original lookup logic and provide a coherent Vita-side equivalent.

Prefer centralized path translation over many unrelated special cases.

---

## 15. Audio rules

Identify the real source first:

```text
OpenSL ES
AudioTrack
OpenAL
SDL
FMOD
native engine mixer
Ogg Vorbis
MP3
WAV/PCM
```

Then implement the smallest correct bridge.

When the engine already performs mixing, exposing its PCM output to `sceAudioOut` is usually preferable to recreating its audio model.

---

## 16. Input rules

Preferred order:

```text
engine's real controller API
→ Android/Xperia/gamepad path
→ synthetic touch
```

Synthetic touch is acceptable when the game itself is touch-driven, but it must use safe slot allocation and the engine's expected coordinate space.

Never feed raw Vita touch identifiers into an Android engine unless the engine's representation is verified compatible.

---

## 17. Real hardware debugging

Use one log file per run, for example:

```text
ux0:data/zombieshooter/logs/log_<run-id>.log
```

Avoid one endlessly reused log because stale captures can lead to false diagnoses.

For crashes where text logging is insufficient, use the real hardware `.psp2dmp` and analyze the unstripped Debug ELF with `vita-parse-core`.

When the fault address lies inside a dynamically loaded Android `.so`:

```text
offset = PC_or_LR - module_load_base
```

Resolve around that offset with `nm`, `objdump` and/or Ghidra.

Do not trust a dynamically guessed module base if it conflicts with addresses logged by the loader.

---

## 18. Verification discipline

Use these states explicitly:

```text
STATICALLY VERIFIED
BUILD VERIFIED
VPK VERIFIED
REAL VITA VERIFIED
GAMEPLAY VERIFIED
```

Examples:

- source inspection proves a JNI function has 4 arguments → `STATICALLY VERIFIED`;
- CMake completes → `BUILD VERIFIED`;
- a VPK exists and packages successfully → `VPK VERIFIED`;
- the user runs it on hardware → `REAL VITA VERIFIED`;
- a feature is actually usable during play → `GAMEPLAY VERIFIED`.

Never promote a result to a stronger state without evidence.

---

## 19. Efficiency rule for Codex

Do not consume a full high-reasoning session by researching every possible failure first.

A good balance is:

```text
classify enough to choose the correct architecture
→ implement the next meaningful checkpoint
→ compile
→ stop when real hardware evidence is required
```

The next hardware build should answer a concrete question.

Examples:

```text
Is NativeActivity the correct lifecycle?
Which lifecycle callback crashes?
Does the renderer produce the first game frame?
Which resource is the first missing asset?
Which JNI callback is required to leave the loading screen?
```

That is more valuable than a giant speculative patch set.

---

## 20. Recommended progress order for Zombie Shooter

```text
1. confirm Android lifecycle
2. confirm loader/ABI correctness
3. survive boot
4. reach first game frame
5. reach title/menu
6. make resource loading reliable
7. start gameplay
8. touch input
9. physical controls
10. audio
11. saves
12. stability
13. performance profiling
14. optimization/polish
```

Do not optimize steps 13–14 while steps 1–7 are still uncertain.
