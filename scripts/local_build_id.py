#!/usr/bin/env python3
import hashlib,subprocess
from pathlib import Path
root=Path(__file__).resolve().parent.parent
sha=subprocess.check_output(['git','rev-parse','--short','HEAD'],cwd=root,text=True).strip()
h=hashlib.sha256()
paths=[root/'CMakeLists.txt']
for directory in ('source','lib/opensles_clear','patches'):
    paths+=sorted((root/directory).rglob('*'))
for p in paths:
    if p.is_file():
        h.update(str(p.relative_to(root)).encode());h.update(p.read_bytes().replace(b'\r\n',b'\n'))
print('local-'+sha+'-'+h.hexdigest()[:10])
