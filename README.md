# Zombie Shooter — PS Vita Port

Android-to-PS-Vita port of **Zombie Shooter** by Sigma Team using `so_loader`, vitaGL, FalsoJNI and FalsoNDK.

## Current target

The active target is now:

```text
Zombie Shooter Free 3.6.1
versionCode: 1161
ABI: armeabi-v7a
package: com.sigmateam.zombieshooter.free
```

The game files are proprietary and are **not included** in this repository or in the VPK.

The verified 3.6.1 ARMv7 native library is:

```text
libzombie_shooter.so
size:   9908972 bytes
SHA1:   f7c7bbfc41f7ed8b76c8c5b1af3e9b0dfdf188c0
SHA256: cb461ac47de79536824e8f7b64fa82293b796bcc159675b4f5ad6304e60bdca1
```

See `docs/ZOMBIE_SHOOTER_3_6_1.md` for the migration and hardware-test notes.

## Requirements

### PS Vita

- HENkaku / Enso
- VitaShell
- `kubridge.skprx` v0.3.1 or newer loaded under `*KERNEL`

### Building locally

Zombie Shooter uses the repository's pinned **SoftFP VitaSDK** configuration:

```bash
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"
```

Do not build this port with the HardFP SDK.

## Prepare your 3.6.1 game data

Use your own **Zombie Shooter 3.6.1 APKPure XAPK, armeabi-v7a**.

From the repository root:

```bash
python3 scripts/extract_game_361.py /path/to/Zombie+Shooter_3.6.1_APKPure.xapk
```

The script validates the exact version and ARMv7 `.so`, then creates:

```text
zombieshooter-3.6.1-vita-data/
├── libzombie_shooter.so
├── GAME_VERSION.txt
└── assets/
    └── ... complete assets tree ...
```

Copy the **contents** of that directory to:

```text
ux0:data/zombieshooter/
```

Result:

```text
ux0:data/zombieshooter/libzombie_shooter.so
ux0:data/zombieshooter/assets/game.res
ux0:data/zombieshooter/assets/game.cfg
ux0:data/zombieshooter/assets/strings.ini
ux0:data/zombieshooter/assets/bundles.config
ux0:data/zombieshooter/assets/... all remaining game assets ...
```

The verified 3.6.1 XAPK does **not** require an OBB.

Do not copy `config.armeabi_v7a.apk` itself to the Vita; the extraction script takes the required `.so` from that split. Do not mix 3.2.3/3.5.3 files with 3.6.1.

For a clean first test, rename the previous data folder before copying 3.6.1:

```text
ux0:data/zombieshooter/ -> ux0:data/zombieshooter_old/
```

## GitHub Actions VPK

The workflow in `.github/workflows/manual-prerelease.yml` builds both:

```text
Zombie-Shooter-Vita-Debug.vpk
Zombie-Shooter-Vita-Release.vpk
```

The VPK contains only the Vita loader and LiveArea resources. **Game assets and the Android `.so` are never packaged.**

For a first 3.6.1 hardware test, install the **Debug VPK** so the bring-up log contains enough information to diagnose missing imports/JNI/NDK behavior.

## Local build

```bash
python3 scripts/prepare_game_361_source.py
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
```

The generated VPK is:

```text
build/zombie_shooter.vpk
```

## 3.6.1 migration policy

3.6.1 has different internal addresses from the older game binary. The first 3.6.1 bring-up intentionally disables old game-version-specific engine/raster/audio/registry hooks and keeps only the generic Android kuser/protobuf compatibility patch.

The 3.6.1 engine contains a real native Android gamepad path (`AndroidJoystickControl`, `InputHandlerNative`, `AInputEvent`/`AKeyEvent`/MotionEvent axes). The port should use that path rather than recreating the old synthetic gameplay-touch workaround.

Do not report a controller/save/performance fix as complete until it has been tested on real PS Vita hardware.

## Runtime data and diagnostics

The loader uses:

```text
ux0:data/zombieshooter/
```

If the first 3.6.1 test fails, preserve the newest files from:

```text
ux0:data/zombieshooter/logs/
ux0:data/zombieshooter/last_init.txt
ux0:data/zombieshooter/ndk_step.txt
```

and the `.psp2dmp` if the Vita creates one.

## Credits

- v-atamanenko — soloader-boilerplate / FalsoJNI / FalsoNDK ecosystem
- TheFloW — so_util / kubridge foundations
- Rinnegatamante — vitaGL and Android-to-Vita porting work
- MetalSyntax and the wider Vita homebrew community — porting methodology and reference implementations

## License

Repository code is licensed under MIT where applicable. Original Zombie Shooter game files remain property of their respective rights holders and are not redistributed here.
