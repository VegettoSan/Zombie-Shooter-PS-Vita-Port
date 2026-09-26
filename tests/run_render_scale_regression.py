#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix="zombie-scale-") as directory:
    for opt in (0,3):
        (Path(directory)/"config.txt").unlink(missing_ok=True)
        out=str(Path(directory)/"test")
        common=["cc","-std=gnu11",f"-O{opt}","-Wall","-Wextra","-Werror"]
        subprocess.run(common+["tests/render_scale_regression.c","-o",out],cwd=root,check=True)
        subprocess.run([out],check=True)
        subprocess.run(common+[f'-DDATA_PATH="{directory}/"',"tests/settings_regression.c","source/utils/settings.c","-o",out],cwd=root,check=True)
        subprocess.run([out],check=True)
