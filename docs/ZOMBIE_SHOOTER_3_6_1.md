# Zombie Shooter 3.6.1 ARMv7 target

This repository now targets **Zombie Shooter Free 3.6.1**, versionCode **1161**, using the **armeabi-v7a** split.

The game files are proprietary and are **not** included in the repository or VPK. Extract them from your own XAPK.

## Verified XAPK identity

Package:

```text
com.sigmateam.zombieshooter.free
```

Version:

```text
versionName 3.6.1
versionCode 1161
```

Required split:

```text
config.armeabi_v7a.apk
```

Verified native library:

```text
path in split: lib/armeabi-v7a/libzombie_shooter.so
size:          9908972 bytes
SHA1:          f7c7bbfc41f7ed8b76c8c5b1af3e9b0dfdf188c0
SHA256:        cb461ac47de79536824e8f7b64fa82293b796bcc159675b4f5ad6304e60bdca1
ELF:           32-bit ARM, EABI5, soft-float, Android 24, NDK r28c
```

The library still exports `ANativeActivity_onCreate` and imports the NativeActivity/input APIs used by the Vita loader.

The 3.6.1 engine also exposes its native joystick path, including:

```text
input::InputHandlerNative::onJoysticEvent(AInputEvent*)
input::AndroidJoystickControl::handleEvent(AInputEvent*)
input::MotionAPIV14::axisValue(AInputEvent const*, int, unsigned int)
AInputEvent_getDeviceId
AInputEvent_getSource
AKeyEvent_getAction
AKeyEvent_getKeyCode
```

This is the controller path the port should reproduce. Do not replace it with the old 3.2.3 gameplay-touch workaround.

## Preparing Vita data

From the repository root:

```bash
python3 scripts/extract_game_361.py /path/to/Zombie+Shooter_3.6.1_APKPure.xapk
```

This creates:

```text
zombieshooter-3.6.1-vita-data/
├── libzombie_shooter.so
├── GAME_VERSION.txt
└── assets/
    ├── game.res
    ├── game.cfg
    ├── strings.ini
    ├── bundles.config
    └── ... all other assets from the base APK
```

Copy the contents to:

```text
ux0:data/zombieshooter/
```

The minimum expected Vita layout is therefore:

```text
ux0:data/zombieshooter/libzombie_shooter.so
ux0:data/zombieshooter/assets/game.res
ux0:data/zombieshooter/assets/game.cfg
ux0:data/zombieshooter/assets/strings.ini
ux0:data/zombieshooter/assets/bundles.config
```

Copy the **entire `assets/` directory**, not only those four files. The verified base APK contains more than two thousand asset files and the engine resolves them through `AAssetManager`.

There is no OBB in the verified 3.6.1 APKPure XAPK.

## Clean migration from the old build

Do not mix 3.2.3/3.5.3 files with 3.6.1.

For the first hardware test, rename the old folder instead of deleting it:

```text
ux0:data/zombieshooter/        -> ux0:data/zombieshooter_old/
```

Then create a fresh:

```text
ux0:data/zombieshooter/
```

and copy only the 3.6.1 files produced by the extraction script.

## Bring-up policy

3.6.1 has different internal addresses from the previous port baseline. The initial 3.6.1 build must **not** install the old version-specific engine hooks, render probes, registry race workaround, raster hooks or optimization hooks merely because symbol names still exist.

For the first 3.6.1 hardware build the loader keeps only the generic Android kuser/protobuf compatibility patch. Further hooks must be re-derived against the 3.6.1 `.so` and enabled one subsystem at a time after real Vita evidence.

Verification states:

```text
STATICALLY VERIFIED: XAPK/ABI/lifecycle/input symbols
BUILD VERIFIED:      GitHub Actions succeeds
VPK VERIFIED:        generated VPK passes unzip/package checks
REAL VITA VERIFIED:  only after installation on physical hardware
GAMEPLAY VERIFIED:   only after the game reaches and plays gameplay correctly
```

## First physical Vita test

Install the Debug VPK first. Confirm, in order:

1. loader accepts the exact 3.6.1 ARMv7 `.so`;
2. relocation/import resolution completes;
3. all `.init_array` constructors complete;
4. `ANativeActivity_onCreate` returns;
5. lifecycle reaches input queue + native window creation;
6. first rendered frame/menu appears;
7. analog and digital gamepad events are recognized;
8. audio, saves and performance are evaluated only after stable gameplay is reached.

If it crashes, return the newest files from:

```text
ux0:data/zombieshooter/logs/
ux0:data/zombieshooter/last_init.txt
ux0:data/zombieshooter/ndk_step.txt
```

and the `.psp2dmp` if one is generated.
