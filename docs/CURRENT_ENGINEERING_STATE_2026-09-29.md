# Zombie Shooter PS Vita — Current engineering state (2026-09-29)

This file is the current handoff for the **Zombie Shooter 3.6.1 build 1161 `armeabi-v7a`** port. It records what has been verified on a real PS Vita, what remains problematic, and experiments that must **not** be repeated without new evidence.

## Canonical target

```text
Zombie Shooter Free 3.6.1
versionCode: 1161
package: com.sigmateam.zombieshooter.free
ABI: armeabi-v7a / ARM32 SoftFP
libzombie_shooter.so size: 9,908,972 bytes
SHA1: f7c7bbfc41f7ed8b76c8c5b1af3e9b0dfdf188c0
SHA256: cb461ac47de79536824e8f7b64fa82293b796bcc159675b4f5ad6304e60bdca1
Vita data: ux0:data/zombieshooter/
```

Do not transfer addresses, hashes, save assumptions or Java/native behavior from 3.2.3 or 3.5.3 to this target.

## Current stable reference

The last physical-Vita baseline before the failed direct-AAsset experiment is:

```text
source state: 6b9ad638c768dd14945ef318a85e7bf7200bcdd9
physical-test VPK: build 59
Release SHA256: 2898570a8b5c64a84d7aed3a77e87be8a64448ea7c5a10fae76dc172c1e6c836
```

After the failed build-66 experiment, `master` was explicitly restored so its source contents compare with **zero changed files** against `6b9ad638...`. The later rollback commits are history only; the functional source state is back to the build-59 baseline.

## What is verified working

### Campaign persistence / Registry

**GAMEPLAY VERIFIED on real Vita.** Progress survives closing and reopening the game; it no longer always returns to the tutorial.

The active design is intentionally simple and preserves the game's native Registry key namespace:

```text
ux0:data/zombieshooter/save/<encrypted-key-hex>.dat
```

Each `.dat` contains the raw Registry value bytes for one key. Do not replace this with a custom aggregate save container unless the actual game contract proves that necessary.

Important recovered native symbols for this exact SO include:

```text
core::Registry::storeEncrypted  0x003FB985 (Thumb)
core::Registry::loadDecrypted   0x003FB055 (Thumb)
core::Registry::encryptKey      0x003FBFF5 (Thumb)
core::Registry::remove          0x003FBEF5
core::Registry::contains        0x003FD351
STRING::c_str                   0x003ED769 (Thumb)
```

The original AES value-encryption path could not be reused safely because its EVP setup failed on this port. The stable implementation keeps the exact native encrypted-key namespace through `Registry::encryptKey()` and stores the value bytes directly.

### Save performance

**GAMEPLAY VERIFIED on real Vita.** Build 52 still caused visible stalls because the filesystem write occurred synchronously on the game thread. Build 59 fixes that.

Current architecture:

```text
Registry change
-> update in-memory map synchronously
-> enqueue persistence mutation
-> return to game immediately
-> background worker writes tmp/bak/dat
```

Properties that must be preserved:

- reads after startup come from the in-memory Registry map;
- writes own/copy their queued key/value data;
- repeated updates of the same key are coalesced so latest value wins;
- clear/remove ordering must not resurrect old keys;
- worker creation failure falls back to the known synchronous path instead of silently losing saves;
- no device-wide `sceIoSync("ux0:", ...)` per value;
- preserve the existing per-key `.dat` format and filenames.

The user confirmed that the ~1 second save freeze disappeared after the asynchronous worker was introduced.

### Logger performance fix

A previous source of apparent save stalls was the logger itself. Quiet Debug/Release used to perform physical `sceIoSyncByFd()` checkpoints from gameplay/error paths; a measured logging block exceeded one second on hardware.

Current rule:

- normal quiet Debug/Release checkpoints may flush userspace buffers but must **not** force physical fsync;
- fatal and explicit `logger_force_sync()` remain durable sync points;
- verbose diagnostic mode may remain more synchronous when intentionally selected.

Do not reintroduce physical fsync on each ERROR/PERF line or periodic quiet-log checkpoint.

### Existing gameplay baseline

The currently preferred baseline is build 59. Saves persist and saves no longer freeze gameplay. Existing music/render/control work predating this document should be preserved unless a new physical-Vita test demonstrates a regression that requires changing it.

## Known remaining issue: loading time

Loading is still slower than desired. The user reported the stable build-59 initial load at roughly **three minutes** in the current 3.6.1 state, although older optimization passes at other stages had reached about one minute.

Do not assume the save system is the dominant cause. Runtime Registry reads after startup are from RAM. The startup cost includes MAP/VID/asset work and must be profiled as a whole.

The current in-memory asset cache is deliberately bounded; do not increase it aggressively without memory measurements because real-Vita logs have shown little free user memory at some checkpoints.

## FAILED EXPERIMENT — build 66 direct `sceIo` AAsset backend

**REAL VITA REGRESSION. DO NOT REPEAT THIS APPROACH AS-IS.**

### Hypothesis

Profiling had attributed a large amount of startup time to successful `AAssetManager_open()` calls. FalsoNDK's generic implementation uses `FILE*` and obtains length with:

```text
fopen
fseek(SEEK_END)
ftell
fseek(SEEK_SET)
```

An experiment therefore replaced the guest AAsset imports with a custom read-only backend using:

```text
sceIoGetstat
sceIoOpen
sceIoRead
sceIoLseek
sceIoClose
```

while attempting to keep the existing LRU/negative cache behavior.

### Result

It made performance **worse**:

- initial loading remained extremely long;
- loading between menus became visibly slower;
- loading entering/leaving levels became visibly slower;
- the user described the whole loading behavior as substantially worse than the build-59 baseline.

The Release hardware log from this test showed a roughly **94 second** gap between early startup and the VID-link phase and later delays on the order of **20–25 seconds** around menu/level transitions.

### Root cause / lesson

The earlier `AAssetManager_open()` timing was not sufficient evidence that stdio itself should be removed.

The engine performs many small reads while parsing `.vid`, maps, menus and other assets. `fread` benefits from stdio buffering. Replacing that path with direct `sceIoRead` meant many tiny logical reads could become real Vita filesystem reads. Avoiding a few seeks during open therefore traded away valuable read buffering and made both startup and runtime transitions worse.

### Hard rule

Do **not** replace FalsoNDK AAsset/`fread` with unbuffered direct `sceIoRead` based only on aggregate open timing.

Any future asset-I/O optimization must preserve or improve read-ahead/buffering and must be A/B tested on real Vita for all of:

```text
cold launch -> main menu
main menu -> gameplay
pause/options/menu transitions
gameplay -> next level/zone
return to main menu
```

A startup-only benchmark is insufficient.

### Rollback

The build-66 backend, patcher and integration were removed. The post-rollback `master` contents were compared against `6b9ad638...` and GitHub reported **0 changed files**.

## What not to do in future passes

1. **Do not change the save format casually.** Current per-key `.dat` persistence is gameplay verified.
2. **Do not move save writes back to the game/render thread.** The asynchronous worker fixed a real hardware freeze.
3. **Do not add per-save or per-log physical fsync.** Hardware showed multi-hundred-millisecond to >1 s sync costs.
4. **Do not interpret every `Registry.loadDecrypted` trace as disk I/O.** After startup the Registry serves values from RAM.
5. **Do not replace buffered AAsset reads with raw direct `sceIoRead` without a buffering strategy.** Build 66 proved this can severely regress every loading transition.
6. **Do not increase the asset cache just because misses exist.** Check Vita memory headroom first.
7. **Do not optimize from one aggregate counter alone.** Correlate phase duration, call pattern and physical-Vita behavior.
8. **Do not claim a performance fix from CI/build success.** Performance changes require real Vita A/B testing.

## Recommended loading work if revisited

Loading optimization is intentionally deferred while shader work proceeds. When revisited, start from build-59 behavior and instrument rather than replace I/O wholesale.

Preferred investigation order:

```text
1. preserve current stdio/FalsoNDK behavior
2. measure MAP/VID phases separately
3. identify repeated opens of the same exact assets
4. measure logical read sizes/counts per asset class
5. distinguish filesystem latency from parser/CPU time
6. examine repeated failed path probes already not covered by negative cache
7. only then consider buffered read-ahead, selective preloading or a persistent hot-asset cache
8. A/B every change on real Vita across launch + in-game transitions
```

If a packed/hot-asset cache is ever attempted, the normal unpacked assets should remain the authoritative source/fallback and the cache must be versioned/invalidatable. It must not silently become another proprietary game-data format.

## Shader work — next active area

The next planned engineering area is shaders/render compatibility, not another loading rewrite.

The repository already has shader cache/inventory instrumentation. Previous physical logs showed shader-cache hits with no evidence that shader compilation alone explains multi-minute loading. Therefore shader work should focus on **correctness and measured runtime cost**, not assume it will solve the entire loading problem.

Required shader workflow:

```text
observe exact glShaderSource inputs
-> hash/deduplicate
-> map which programs are actually used in gameplay
-> preserve known-good cache behavior
-> dump/transpile only observed unknown/problematic shaders
-> validate with Vita toolchain
-> A/B visual correctness and FPS on real Vita
```

## Verification vocabulary

Use these labels strictly:

```text
STATICALLY VERIFIED
BUILD VERIFIED
VPK VERIFIED
REAL VITA VERIFIED
GAMEPLAY VERIFIED
REGRESSION / REJECTED
```

The direct-AAsset build-66 experiment is explicitly **REGRESSION / REJECTED**. The async campaign-save behavior in build 59 is **GAMEPLAY VERIFIED**.

## Fast handoff summary

```text
Target: Zombie Shooter 3.6.1 build 1161 armeabi-v7a
Stable behavior reference: build 59 / source 6b9ad638...
Save persistence: working across relaunch
Save hitch: fixed by async persistence worker
Logger fsync hitch: fixed; quiet logging must not physical-fsync gameplay thread
Loading: still too slow, but build-66 direct sceIo experiment made it worse
Direct AAsset sceIo backend: REJECTED and rolled back
Next active area: shaders/render correctness + measured performance
```
