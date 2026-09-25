# Run #9 — Release verification and new-zone crash disassembly

Date: 2026-09-25

State: **BUILD VERIFIED / STATIC CRASH ROOT CAUSE NARROWED / HARDWARE REVALIDATION PENDING**

## Run identity

GitHub Actions run #9:

```text
run: 9
commit: 2ca93af12b396ab1d2addcb3cbb4af48665c9a23
tag: vita-test-9-2ca93af
build id embedded in logs: 2ca93af
```

The canonical game SO analyzed by CI matched the expected SHA-256:

```text
5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7
```

## Release variant is now proven by compiler flags

The artifact contains `build-flags.txt` captured from the actual CMake targets.

Debug:

```text
FALSOJNI_DEBUGLEVEL=0
ZOMBIE_BUILD_VARIANT="Debug"
ZOMBIE_DEBUG_BUILD=1
SO_UTIL_VERBOSE=1
ZOMBIE_THREAD_TRACE=1
ZOMBIE_STALL_DUMP=1
```

Release:

```text
FALSOJNI_DEBUGLEVEL=4
ZOMBIE_BUILD_VARIANT="Release"
ZOMBIE_RELEASE_BUILD=1
```

Release does not contain `SO_UTIL_VERBOSE=1`, `ZOMBIE_DEBUG_BUILD=1`, thread trace, or stall-dump defines.

Therefore the run #9 Release VPK is BUILD VERIFIED as the intended quiet performance variant. This removes the ambiguity that existed in run #8.

The exact retrospective cause of run #8's Debug-like runtime log is still not provable because that run did not preserve `flags.make`. Do not claim that the user installed the wrong VPK; the user explicitly confirmed Release was installed.

## Runtime identity added

Every new persistent log now contains:

```text
=== BUILD variant=<Debug|Release> id=<short sha> ===
```

A valid run #9 Release hardware test must begin with:

```text
=== BUILD variant=Release id=2ca93af ===
```

## PSVshell clocks

The port no longer calls `scePowerSet*ClockFrequency()` during startup. It preserves the externally selected PSVshell profile and records:

```text
[PERF] clocks_preserved arm=... bus=... gpu=... xbar=...
```

Previous tests with PSVshell set to maximum were not valid max-clock A/B tests because older builds forced ARM/BUS/GPU/XBAR back to 444/222/222/166.

## Crash symbol resolved

The real-Vita dump reported:

```text
PC   = 0x98515AE4 = SO + 0x00515AE4
LR   = 0x98515A5F = SO + 0x00515A5F
DFAR = 0x07D36E9C
```

`addr2line` against the canonical SO resolves both PC and LR to:

```text
VID_SOFTWARE16::DrawToVid(
    SpriteInfo const*,
    VID_TEXCOOR const*,
    TEXTURE*,
    TEXTURE*)
```

This is direct evidence that the crash occurs in the software VID rendering path, not in VitaGL, EGL, or a generic system module.

## Exact faulting instruction

Relevant Thumb code:

```asm
515ad4  ldr.w  r0, [fp, #0x484]       ; pointer to a 32-bit table
515ad8  ldrsh.w r2, [r3, #0x18]       ; signed index from SpriteInfo
515adc  ldr.w  r1, [fp, #0x48c]       ; VID data base pointer
515ae0  ldr.w  r0, [r0, r2, lsl #2]   ; r0 = table[index]
515ae4  ldrsh  r2, [r1, r0]           ; DATA ABORT here
```

Registers from the dump at the fault:

```text
FP/R11 = 0x848932D8
R3     = 0x866D8508
R2     = 0x00000000
R1     = 0x86932D80
R0     = 0x8140411C
```

The effective address is therefore:

```text
0x86932D80 + 0x8140411C = 0x107D36E9C
32-bit wrap             = 0x07D36E9C
```

That matches the CPU's DFAR **exactly**.

So the dump and disassembly now prove the mechanical cause of the crash: `VID_SOFTWARE16::DrawToVid` reads a 32-bit value from an internal table and uses it as a byte offset from the VID data base. For index 0, the table supplied `0x8140411C`, which is not a plausible small data offset and produces the unmapped fault address.

## Object state reconstructed from the core

From the `VID_SOFTWARE16` object at `FP=0x848932D8`:

```text
this + 0x484 = 0x863E7C78   ; table pointer
this + 0x488 = 0x00000400   ; likely table count/size; semantic name not yet proven
this + 0x48C = 0x86932D80   ; data base used by the faulting instruction
```

From the `SpriteInfo` object at `R3=0x866D8508`:

```text
*(int16_t *)(SpriteInfo + 0x18) = 0
```

Thus the bad value came from table element 0.

The core did not capture the page containing `0x863E7C78`, so the table memory itself cannot be dumped retrospectively. The value of element 0 is nevertheless known because `R0=0x8140411C` is the value loaded immediately before the fault.

The first captured bytes at the VID data base `0x86932D80` are zero-filled. This is compatible with a partially initialized/empty resource, but does not alone prove why initialization failed.

## Relationship to missing VID resources

Immediately before the crash, the game log reported failures for resources including:

```text
vid/2010.vid
vid/1071.vid
vid/413.vid
vid/2020.vid
```

with errors such as:

```text
Can't open
Can't load PAL section
Can't load DATA section
Can't read cadr data
```

The crash is now proven to occur inside `VID_SOFTWARE16::DrawToVid`, using an invalid entry from an internal VID offset table.

This substantially strengthens the resource hypothesis:

```text
missing/incomplete VID resource
→ VID_SOFTWARE16 remains partially initialized or contains stale table data
→ DrawToVid reads invalid table[0] offset 0x8140411C
→ base + offset wraps to 0x07D36E9C
→ Data Abort
```

However, the first arrow is still not formally proven. A future targeted hook should record the relevant `VID_SOFTWARE16` table/base fields at load/draw time and only then decide whether to guard/skip malformed VID draws.

## What is proven now

- Crash PC and LR belong to `VID_SOFTWARE16::DrawToVid`.
- The exact faulting instruction is identified.
- The effective address reconstructed from R0/R1 equals DFAR exactly.
- SpriteInfo selected table index 0.
- The table entry used for that index was `0x8140411C`.
- That value is invalid as the byte offset used by this function.
- Missing `.vid` resources occur immediately before this crash path.

## What is NOT proven yet

- Which exact missing `.vid` instance owns the crashing `VID_SOFTWARE16` object.
- Whether `commonAssets`/Play Asset Delivery is the only reason the VID data is incomplete.
- Whether the bad table value comes from uninitialized memory, stale/freed memory, a parser failure, or a different compatibility bug.
- A safe universal upper bound for valid VID table offsets.

Do not patch the canonical `demo/libzombie_shooter.so` in place.

## Next controlled hardware test

Use the run #9 Release VPK without adding a VID guard yet. The first objective is to validate the corrected Release/performance instrumentation in a safe part of the tutorial.

Expected log markers:

```text
=== BUILD variant=Release id=2ca93af ===
[PERF] clocks_preserved ...
[PERF] mem phase=before_vgl ...
[PERF] mem phase=after_vgl ...
[PERF] mem phase=runtime ...
```

For this first run, remain in the already-known safe tutorial area for roughly 60–120 seconds and do not intentionally enter the new crash zone. Return the new log. Once build identity, clocks and memory telemetry are confirmed, add a targeted VID diagnostic/guard as the next isolated change.
