# MetalSyntax Android -> PS Vita porting guide

This is the repository's mandatory methodology entry point for Android->Vita work.

## Mandatory current handoff

Before editing save, JNI, filesystem ABI, renderer, shaders, performance, asset loading, memory, input, audio or binary patches, **read this document and then read**:

```text
docs/METALSYNTAX_TOOLKIT_FINDINGS_2026-09-28.md
```

That handoff contains the current Zombie Shooter **3.6.1 build 1161 ARMv7** findings, including two guest ABI bugs found in this repository (`stat/stat64` and `dirent`), the Registry/SharedPreferences investigation map, performance profiling strategy and the shader-toolkit plan.

Do not rely on older 3.2.3/3.5.3 assumptions when they conflict with the 3.6.1 handoff or later physical-Vita evidence.

---

## Core rule

A PS Vita soloader port is not a generic Android emulator. Reproduce the exact behavior the game's Android binary expects, with the smallest compatible Vita-side surface.

Use this evidence loop:

```text
exact APK/XAPK + exact .so
-> JADX / manifest / symbols / disassembly
-> identify real lifecycle + ABI contract
-> implement one minimal compatibility change
-> host/build regression
-> physical Vita test
-> log / .psp2dmp
-> root cause
-> repeat
```

A build succeeding is not hardware verification.

---

## Reference projects

Useful public MetalSyntax references include:

```text
MetalSyntax/psvita-port-toolkit-cli
MetalSyntax/Zenonia4-psvita-port
MetalSyntax/ILLUSIA-2-Vita
MetalSyntax/Advena-Vita
MetalSyntax/Asphalt-5-Vita
MetalSyntax/Asphalt-6-Vita
MetalSyntax/Shadow-Guardian-vita
MetalSyntax/Dungeon-Hunter-2-vita
MetalSyntax/Sacred-Odyssey-vita
MetalSyntax/Gangstar-Miami-Vindication-Vita
MetalSyntax/The-Impossible-Game-vita
MetalSyntax/prince-of-persia-classic-psvita-port
```

Research selectively. Reuse methodology and game-agnostic compatibility patterns only after confirming ABI/engine/lifecycle similarity.

**Never copy another game's binary offsets or assume its JNI/save object layout matches Zombie Shooter.**

---

## Exact-binary rule

Every game-version-specific hook must be tied to the exact target binary by one or more of:

```text
exported/dynamic symbol
verified function address
prologue/signature guard
Ghidra/objdump evidence
physical crash/log evidence
```

For Zombie Shooter 3.6.1, the current target library is documented in `METALSYNTAX_TOOLKIT_FINDINGS_2026-09-28.md`.

Do not derive a global offset delta from an older version and apply it across the binary.

---

## JNI and Android ABI rule

JNI signatures and guest C layouts are correctness-critical.

Verify:

```text
static vs instance
jclass vs jobject
argument count and promoted variadic types
return type
jstring / jbyteArray representation
legacy Dalvik direct-layout access vs real FalsoJNI objects
sizeof / offsetof / packing / alignment of Android structs
```

Do not use Vita/newlib typedefs inside guest-visible Android structures unless their width/layout has been proven identical.

The repository now protects ARM32 Bionic `stat64` and `dirent` with fixed-width definitions and compile-time layout assertions. Preserve those invariants.

---

## Save rule

Do not define "save system" as "make a file appear".

Reconstruct the exact 3.6.1 persistence chain:

```text
RegistryPrivate / Registry
-> Java/FalsoJNI SharedPreferences and RegistryEnumerator callbacks
-> batching / sync / async flush behavior
-> optional dump generation/load
-> file I/O / rename / fsync / stat validation
-> engine acceptance on the next launch
```

The 3.6.1 library exposes a rich Registry API (`queueFlush`, `begin/endBatchUpdate`, `setRegSync/Async`, `createDump`, `generateDump`, `loadDump`, etc.). Do not collapse it to a generic key/value file without proving that is semantically sufficient.

A save is only `GAMEPLAY VERIFIED` after the user advances, exits the app, relaunches and the game resumes the persisted progress rather than returning to the tutorial.

---

## Graphics and shader rule

Zombie Shooter 3.6.1 uses real programmable GLES APIs and contains GLSL ES shader sources, while important gameplay rendering also uses a software work surface uploaded through GLES.

Therefore:

- profile before deciding whether CPU software rendering, uploads, GPU work or shader compilation is dominant;
- keep VitaGL as the compatibility baseline;
- dump/deduplicate real shaders before attempting precompiled Cg replacements;
- use `glslangValidator -> SPIR-V -> SPIRV-Cross -> Cg -> psp2cgc` only for shaders actually observed from this game;
- never promise gameplay FPS from shader precompilation alone.

The detailed shader plan is in the current toolkit findings document.

---

## Performance rule

Measure on **real Vita** and optimize the active path.

Prefer eliminating work over making the same work slightly faster:

```text
redundant framebuffer/texture uploads
redundant copies and format conversions
repeated decode/compile work
failed filesystem probes
unnecessary locks/synchronization
unnecessary logging in Release
```

Useful profiling concepts from `psvita-port-toolkit-cli`:

```text
perf-telemetry: FPS, frame time, p95/p99, stutters
mem-profile: live/peak heap and allocations surviving checkpoints
logs-live: low-latency diagnostic events
soak-test: long-running hang/stability checks
```

`sceGxmFinish()` timing is profiling-only because forcing GPU completion changes frame behavior.

Do not ship per-frame/per-allocation diagnostic traffic in Release.

---

## Input rule

Preferred order:

```text
engine's native Android gamepad path
-> verified Android key/axis events
-> synthetic touch only where the original game is genuinely touch-driven
```

Zombie Shooter 3.6.1 contains native joystick handling. Verify `AInputEvent` source/device, key codes and motion axes end-to-end rather than layering more button translations blindly.

---

## Assets and paths

Reconstruct original access semantics from the exact game:

```text
AAssetManager
fopen/open/stat/access
APK assets/splits
Android private/external paths
proprietary archives
Java resource helpers
```

Prefer centralized path translation. Do not globally transcode assets until profiling proves a target and the consuming path is understood.

---

## Toolkit usage policy

The MetalSyntax toolkit is an **external engineering tool**, not a runtime dependency of this port.

Good uses:

```text
jni-analyze
so-patch scans
symbol/disassembly helpers
alignment checks
shader transpilation experiments
performance/memory telemetry
crash context export
```

Generated output is a starting point, not authority. Review every generated JNI stub, patch address and shader result against the exact game binary.

Do not vendor the whole toolkit into this repository.

---

## Real hardware policy

Vita3K is not authoritative for this soloader/kubridge path.

Use:

```text
STATICALLY VERIFIED
BUILD VERIFIED
VPK VERIFIED
REAL VITA VERIFIED
GAMEPLAY VERIFIED
```

Never promote a result to a stronger state without evidence.

For a crash, preserve the Debug ELF and request the real `.psp2dmp` when logs are insufficient. Resolve dynamic game addresses as offsets from the logged SO base and inspect the exact function with symbols/objdump/Ghidra.

---

## Next-work ordering for Zombie Shooter 3.6.1

```text
1. keep current 3.6.1 recovery build as hardware checkpoint
2. validate corrected Bionic filesystem ABI
3. reconstruct Registry/SharedPreferences semantics
4. verify campaign persistence end-to-end
5. compare Release FPS to older baseline
6. profile actual active render phases if still slower
7. add optional UDP performance telemetry
8. dump/deduplicate real glShaderSource inputs in Debug
9. transpile/validate only observed shaders
10. add memory profiling before large caches
```

If a proposed change does not help answer one of those concrete questions, defer it until there is evidence it belongs on the critical path.
