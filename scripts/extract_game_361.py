#!/usr/bin/env python3
"""Extract the user-owned Zombie Shooter 3.6.1 ARMv7 data required by the Vita port."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil
import sys
import zipfile

PACKAGE = "com.sigmateam.zombieshooter.free"
VERSION_NAME = "3.6.1"
VERSION_CODE = "1161"
SO_PATH_IN_SPLIT = "lib/armeabi-v7a/libzombie_shooter.so"
SO_SIZE = 9_908_972
SO_SHA1 = "f7c7bbfc41f7ed8b76c8c5b1af3e9b0dfdf188c0"
SO_SHA256 = "cb461ac47de79536824e8f7b64fa82293b796bcc159675b4f5ad6304e60bdca1"


def digest(data: bytes, name: str) -> str:
    h = hashlib.new(name)
    h.update(data)
    return h.hexdigest()


def fail(message: str) -> "NoReturn":
    raise RuntimeError(message)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Prepare ux0:data/zombieshooter data from Zombie Shooter 3.6.1 XAPK"
    )
    parser.add_argument("xapk", type=Path, help="Zombie Shooter 3.6.1 APKPure XAPK")
    parser.add_argument(
        "-o", "--output", type=Path, default=Path("zombieshooter-3.6.1-vita-data"),
        help="output directory (default: %(default)s)",
    )
    args = parser.parse_args()

    if not args.xapk.is_file():
        fail(f"XAPK not found: {args.xapk}")

    out = args.output.resolve()
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    with zipfile.ZipFile(args.xapk) as xapk:
        try:
            manifest = json.loads(xapk.read("manifest.json"))
        except KeyError as exc:
            fail("XAPK has no manifest.json")

        if manifest.get("package_name") != PACKAGE:
            fail(f"wrong package: {manifest.get('package_name')!r}")
        if str(manifest.get("version_name")) != VERSION_NAME:
            fail(f"wrong version_name: {manifest.get('version_name')!r}; expected {VERSION_NAME}")
        if str(manifest.get("version_code")) != VERSION_CODE:
            fail(f"wrong version_code: {manifest.get('version_code')!r}; expected {VERSION_CODE}")

        split_entries = {entry.get("id"): entry.get("file") for entry in manifest.get("split_apks", [])}
        base_name = split_entries.get("base") or f"{PACKAGE}.apk"
        arm_name = split_entries.get("config.armeabi_v7a") or "config.armeabi_v7a.apk"
        if base_name not in xapk.namelist():
            fail(f"base APK missing from XAPK: {base_name}")
        if arm_name not in xapk.namelist():
            fail(f"ARMv7 split missing from XAPK: {arm_name}")

        base_bytes = xapk.read(base_name)
        arm_bytes = xapk.read(arm_name)

    # Native library from the ARMv7 split.
    with zipfile.ZipFile(io.BytesIO(arm_bytes)) as arm_apk:
        try:
            so_data = arm_apk.read(SO_PATH_IN_SPLIT)
        except KeyError:
            fail(f"{SO_PATH_IN_SPLIT} missing from {arm_name}")

    if len(so_data) != SO_SIZE:
        fail(f"unexpected libzombie_shooter.so size: {len(so_data)}; expected {SO_SIZE}")
    if digest(so_data, "sha1") != SO_SHA1:
        fail("libzombie_shooter.so SHA1 does not match the verified 3.6.1 ARMv7 build")
    if digest(so_data, "sha256") != SO_SHA256:
        fail("libzombie_shooter.so SHA256 does not match the verified 3.6.1 ARMv7 build")
    (out / "libzombie_shooter.so").write_bytes(so_data)

    # The game reads its runtime content through AAssetManager; preserve the full assets tree.
    asset_count = 0
    asset_bytes = 0
    with zipfile.ZipFile(io.BytesIO(base_bytes)) as base_apk:
        for info in base_apk.infolist():
            if info.is_dir() or not info.filename.startswith("assets/"):
                continue
            rel = Path(info.filename)
            target = out / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            data = base_apk.read(info)
            target.write_bytes(data)
            asset_count += 1
            asset_bytes += len(data)

    required = ["assets/game.res", "assets/game.cfg", "assets/strings.ini", "assets/bundles.config"]
    missing = [name for name in required if not (out / name).is_file()]
    if missing:
        fail("required game assets missing after extraction: " + ", ".join(missing))

    (out / "GAME_VERSION.txt").write_text(
        "Zombie Shooter Free 3.6.1\n"
        "versionCode=1161\n"
        "abi=armeabi-v7a\n"
        f"libzombie_shooter.so.size={SO_SIZE}\n"
        f"libzombie_shooter.so.sha1={SO_SHA1}\n"
        f"libzombie_shooter.so.sha256={SO_SHA256}\n",
        encoding="utf-8",
    )

    print(f"Prepared: {out}")
    print(f"libzombie_shooter.so: {SO_SIZE} bytes, SHA256 {SO_SHA256}")
    print(f"assets: {asset_count} files, {asset_bytes} bytes")
    print("Copy the CONTENTS of this directory to ux0:data/zombieshooter/ on the Vita.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, OSError, zipfile.BadZipFile, json.JSONDecodeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
