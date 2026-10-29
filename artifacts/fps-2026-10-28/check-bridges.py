from pathlib import Path
import subprocess,tempfile
root=Path('/home/vegettosandev/Zombie-Shooter-PS-Vita-Port')
for relative in ['tests/run_jni_field_regression.sh','tests/run_gamepad_regression.sh','tests/run_asset_index_regression.sh']:
    with tempfile.NamedTemporaryFile(mode='w',dir=root/'tests',suffix='.sh',delete=False) as f:
        path=Path(f.name);f.write((root/relative).read_text())
    try:
        subprocess.run(['bash',str(path)],cwd=root,check=True)
    finally:
        path.unlink()
