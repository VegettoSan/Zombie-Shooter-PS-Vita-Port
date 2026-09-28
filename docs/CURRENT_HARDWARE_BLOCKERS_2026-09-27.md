# Current real-Vita blockers and investigation priorities — 2026-09-27

This document is the current hardware-tested handoff for the next Codex pass. Read it together with `AGENTS.md`, `docs/METALSYNTAX_PORTING_GUIDE.md`, `PORT_STATUS.md`, `port_progress.md`, and the focused input/performance notes.

## Baseline under test

Current tested repository baseline before this documentation pass:

`5e246c97bbf1a8de4858abb4843757dd980ed271`

The game boots, reaches gameplay, audio/music work, and the project is already far beyond the old boot/crash bring-up state. Do not treat old early-port crash notes as the current baseline.

The canonical Android game library remains immutable:

`demo/libzombie_shooter.so`

SHA-256:

`5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7`

Do not patch, replace, strip, move, rename, or regenerate that reference file. Runtime hooks or compatibility shims must remain justified and guarded.

## What was tested on a real PS Vita

The user tested the VPK built from the current post-input/save baseline.

The following runtime configuration subjectively improved FPS:

```text
log_mode 0
asset_cache_mib 16
software_width 864
framebuffer_565 0
software_frameskip 1
```

This configuration is useful as the next performance A/B baseline, but it is not automatically correct for visual fidelity. In particular, `software_frameskip 1`, renderer reuse, texture upload changes, framebuffer format handling, or timing interactions must be considered when diagnosing flicker.

### Performance state

- FPS are better than older baselines with the configuration above.
- There is still a large and obvious FPS drop when dynamic lighting is active, including flashlight/light-heavy gameplay.
- Previous profiling proved that some earlier light optimizations targeted code paths that were not actually used by the tested scene. Do not optimize an unobserved light path again.
- Initial loading remains longer than desirable. Earlier logs already showed large asset-open costs and many failed probes during startup, while normal gameplay is less I/O-bound.

### Visual state

- A small number of textures or sprites visibly flicker on real Vita.
- The problem is not reported as global corruption; most rendering is stable.
- Treat this as a correctness bug first. Do not hide it by disabling effects or globally reducing fidelity.
- Re-test with `software_frameskip`, framebuffer format, texture upload/reuse/COW, palette updates, transient render targets, and producer/consumer synchronization isolated one variable at a time.

### Controller state

The failed synthetic/gameplay-touch experiment was removed and the previous strict/hybrid path was restored. That restoration succeeded, but the original digital-control problem remains.

Real Vita observations from the known broken mapping:

- left and right analog sticks work;
- Cross/Circle/Square/Triangle do not behave as the game expects;
- L can trigger flashlight and grenade together;
- R appears to trigger another gameplay action such as healing;
- only one useful D-pad direction was observed, associated with healing;
- rear-left heals;
- rear-right throws a grenade and can also display current map/level information;
- strict and hybrid did not produce the expected Xbox controller behavior on hardware.

The target logical layout is the Xbox-style layout shown by Zombie Shooter itself:

- left stick: move/navigation;
- right stick: shoot/aim according to the game's controller UI;
- A: select/continue;
- B: back;
- Y: buy ammo;
- LB: next weapon;
- RB: previous weapon;
- LT: use medkit;
- RT: use grenade;
- D-pad up: medkit;
- D-pad down: grenade;
- D-pad left: previous weapon;
- D-pad right: next weapon.

Do not invent a gameplay action for X when the game UI does not show one.

The fact that both analog sticks work while digital actions are wrong is important evidence: the MotionEvent/axis path is substantially healthier than the digital KeyEvent/button path. Reconstruct the game's actual Android controller path from the XAPK/DEX and native code instead of adding another translation layer blindly.

### Save/progress state

Commit `5e246c97...` added a local JNI emulation for Android preferences/registry enumeration and intended to persist strings under:

`ux0:data/zombieshooter/shared_preferences.bin`

Real Vita result:

- that file was not observed after gameplay;
- progress did not persist;
- relaunch still starts from the tutorial.

Therefore the save system is **NOT FIXED** and the current preferences implementation must not be treated as proven or necessarily as the game's real save backend.

Possible explanations that must be distinguished with evidence rather than guessed:

1. the game never reaches the emulated SharedPreferences path during save;
2. the Java/JNI method descriptors, receiver identity, object lifetime, callbacks, or return values do not match the XAPK;
3. progress uses a different Android persistence API or a combination of APIs;
4. progress is written through normal file I/O to Android app/private/external-storage paths that are not mapped correctly;
5. RegistryEnumerator is for configuration/keys but not the actual campaign save;
6. encryption/identity/package-path behavior prevents a valid save from being accepted on the next boot;
7. a write/rename/fsync/stat/access path is missing or fails silently.

Instrument the real write path. Do not merely create `shared_preferences.bin` artificially; a successful test is that the game itself writes valid state and reloads it on the next process launch.

## Local Android source material available to Codex

The user's local working environment contains the complete game material needed for reverse engineering:

- the original XAPK/APK package;
- the canonical Android ARM `.so`;
- extracted assets/data from the XAPK;
- repository sources and current Vita compatibility layers.

Codex should locate and inspect those local files directly. Use JADX/smali, `readelf`, `nm`, `objdump`, `strings`, Ghidra or equivalent targeted disassembly as appropriate.

For every uncertain Android behavior, prefer evidence from the exact shipped game version over assumptions from generic Android documentation.

## External-reference policy

Researching third-party Android-to-Vita ports is explicitly allowed and encouraged when it helps establish a known working pattern.

Useful families include ports using FalsoJNI/FalsoNDK, NativeActivity, VitaGL and similar Android ARM loaders, including work by experienced Vita port developers such as Rinnegatamante/MetalSyntax and other public ports.

However:

- first establish that the compared port uses a genuinely similar Android lifecycle/API path;
- do not copy binary offsets or game-specific patches;
- do not import code without checking license/provenance;
- clone reference repositories under `/tmp/...` or otherwise outside this repository;
- adapt architecture and semantics, not magic constants.

## Required investigation: save system

Before implementing another save fix:

1. Search the XAPK DEX/smali for `SharedPreferences`, `getPreferences`, `getSharedPreferences`, `RegistryEnumerator`, `File`, `FileOutputStream`, `RandomAccessFile`, SQLite, protobuf persistence, external storage, internal app paths, `rename`, `delete`, and native save wrappers.
2. Trace Java methods into registered/exported native methods and into the canonical SO.
3. Inspect file-related imports and path-building strings in the SO.
4. Add bounded Debug instrumentation around file open/create/write/rename/stat/access/unlink/fsync and relevant JNI preference calls so one hardware log can show what the game actually attempts.
5. Reproduce Android path semantics under `ux0:data/zombieshooter/` without changing the logical paths returned to the game more than necessary.
6. Verify process-restart persistence, not only same-process reads.
7. Add host regressions for any emulated persistence API, but keep verification state as BUILD/STATIC until hardware proves campaign progress survives restart.

Acceptance criterion: complete some progress beyond the tutorial, fully close the game, relaunch, and continue from the saved state without replaying the tutorial.

## Required investigation: controller input

Do not solve input by manually binding PC keyboard actions unless static analysis proves that is the actual Android gamepad path.

1. Inspect the exact XAPK controller classes and native input code.
2. Determine which Android APIs the game consumes for digital buttons: `AKeyEvent_getKeyCode`, scan code, source/device id, meta state, repeat count, `AMotionEvent_getButtonState`, HAT axes, Java `KeyEvent`, controller enumeration, or engine-specific callbacks.
3. Determine whether the game expects Android `KEYCODE_BUTTON_*`, Linux/Android scan codes, button-state bitfields, or another mapping.
4. Compare FalsoNDK's synthesized event fields with real Android gamepad events required by this game.
5. Inspect the existing `patches/falso_ndk.patch`, `source/utils/gamepad.c`, and `source/java.c` together; do not treat the mapping table alone as the whole input system.
6. Add a deterministic Debug trace that logs one line per digital transition with physical Vita button, logical Xbox control, Android device/source/type/action/keycode/scancode/button state, and the branch by which it enters the game.
7. Build a regression from the discovered contract, not from the current incorrect behavior.

Acceptance criterion: on real Vita the game's own controller UI/gameplay semantics match the target Xbox layout above, without one physical press causing unrelated duplicate actions.

## Required investigation: texture/sprite flicker

Treat the flicker as a synchronization/lifetime/state problem until proven otherwise.

Investigate at least:

- whether flickering assets share upload format, palette path, texture dimensions, alpha mode, dynamic updates, or render layer;
- `glTexImage2D`/`glTexSubImage2D` compatibility and VitaGL storage lifetime;
- copy-on-write/reuse optimizations already added by prior performance passes;
- stale guest texture IDs or binding-cache invalidation;
- framebuffer/render-target reuse and format conversion;
- software renderer buffer ownership and producer/consumer handoff;
- frame skipping causing a buffer or texture to be reused before the consumer has finished;
- palette/gamma buffers being mutated while still referenced;
- alpha/sprite ordering and discard behavior.

Create a small diagnostic mask/config flag for A/B tests rather than disabling broad renderer features permanently.

Acceptance criterion: the affected sprites/textures remain stable through repeated scenes while retaining current visual effects and without a major performance regression.

## Required investigation: lighting FPS drops

Use profiling from the scene that actually drops FPS.

1. Capture per-window/per-frame timing when flashlight/lights are off vs on in as similar a scene as possible.
2. Separate CPU software renderer, script tick, light construction, light draw/composite, texture upload, synchronization waits and GPU submission where measurable.
3. Confirm which exact SO functions/branches are hot before optimizing.
4. Reuse prior profiling infrastructure where possible; do not stack another unused NEON path.
5. Optimize exact math/data movement only after reconstructing the canonical behavior and add bit/ABI regressions when replacing native routines.
6. Prefer eliminating redundant work, copies, format conversions, repeated setup and waits before reducing effect quality.

Acceptance criterion: visibly smaller FPS delta between comparable light-off/light-on scenes with no broken flashlight, explosions, shadows, blending or color output.

## Required investigation: loading time

Earlier hardware logs already showed startup dominated in part by asset path probing/open cost, including many failed opens.

Investigate:

- path normalization/case handling that may cause repeated failed probes;
- duplicate opens/seeks/whole-file reads;
- whether the asset index/cache covers the actual high-cost directories;
- whether compressed XAPK assets are being emulated inefficiently after extraction;
- shader compilation/linking only where still significant;
- repeated game initialization work caused by missing persistence/registry data;
- safe cache prewarming or negative lookup caching based on measured repeated paths.

Do not preload the whole game into RAM blindly.

Acceptance criterion: reduce initial loading time measurably on real Vita and document before/after timing.

## Current priorities for the next implementation pass

All five issues are in scope, but work must remain evidence-driven:

1. make save/progress survive a full restart;
2. make digital controls match the game's Xbox layout;
3. remove texture/sprite flicker;
4. reduce the real lighting FPS penalty and improve general FPS further;
5. reduce startup/loading time where measurement justifies it.

Performance changes are now authorized because meaningful gameplay is already reached, but visual/gameplay correctness must not be traded away silently.

## Preserve already-working systems

Do not regress these while solving the blockers:

- current playable NativeActivity/FalsoJNI/FalsoNDK boot path;
- original M4A music path and currently working audio/SFX;
- existing VitaGL renderer bring-up;
- the canonical immutable SO reference;
- reproducible SoftFP build setup;
- current asset cache/index work unless measurement proves a specific change is required;
- logs and performance instrumentation needed for hardware A/B tests.

## Verification vocabulary

Use these states literally:

- `STATICALLY VERIFIED`
- `BUILD VERIFIED`
- `VPK VERIFIED`
- `REAL VITA VERIFIED`
- `GAMEPLAY VERIFIED`

A host test or successful VPK build does not prove controller semantics, save persistence, flicker removal, load time or FPS on real Vita.

## Deliverables expected from the next Codex pass

The final Codex report should include:

- exact Android/XAPK evidence found for saves and controller input;
- external reference repositories used and why they are relevant;
- measured root cause for the flicker and lighting slowdown;
- files changed and why;
- host/ARM regression results;
- Debug and Release build status;
- VPK paths;
- all new config/debug flags and their defaults;
- a concise physical-Vita test matrix for save, controls, flicker, lights, FPS and load time;
- no claim of a hardware fix until the user tests it.
