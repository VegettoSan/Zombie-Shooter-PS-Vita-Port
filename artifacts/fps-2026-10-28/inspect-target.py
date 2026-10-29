import hashlib, io, json, zipfile
from pathlib import Path
root=Path('/home/vegettosandev/Zombie-Shooter-PS-Vita-Port')
out=root/'artifacts/fps-2026-10-28'
with zipfile.ZipFile(root/'Zombie+Shooter_3.6.1_APKPure.xapk') as z:
    m=json.loads(z.read('manifest.json'))
    assert str(m['version_name'])=='3.6.1' and str(m['version_code'])=='1161'
    with zipfile.ZipFile(io.BytesIO(z.read('config.armeabi_v7a.apk'))) as arm:
        data=arm.read('lib/armeabi-v7a/libzombie_shooter.so')
    assert len(data)==9908972
    assert hashlib.sha256(data).hexdigest()=='cb461ac47de79536824e8f7b64fa82293b796bcc159675b4f5ad6304e60bdca1'
    (out/'libzombie_shooter.so').write_bytes(data)
print('3.6.1 / 1161 ARMv7 verified; canonical SO extracted for read-only analysis')
