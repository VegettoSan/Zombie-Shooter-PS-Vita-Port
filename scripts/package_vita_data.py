"""Package locally extracted Android game data for ux0:data/ on a real Vita."""

import argparse
from pathlib import Path
from zipfile import ZIP_STORED, ZipFile


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=Path("data"))
    parser.add_argument("--output", type=Path, default=Path("build-session-debug/vita-data.zip"))
    args = parser.parse_args()

    source = args.data.resolve()
    assets = source / "assets"
    so = source / "libzombie_shooter.so"
    for required in (so, assets / "game.res", assets / "bundles.config"):
        if not required.is_file():
            parser.error(f"required game file is missing: {required}")

    files = sorted(path for path in assets.rglob("*") if path.is_file())
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(output, "w", compression=ZIP_STORED, allowZip64=True) as archive:
        archive.write(so, "zombieshooter/libzombie_shooter.so")
        for path in files:
            name = path.relative_to(assets).as_posix()
            archive.write(path, f"zombieshooter/assets/{name}")

    print(f"{output}: {len(files)} assets + libzombie_shooter.so ({output.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
