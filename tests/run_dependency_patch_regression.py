#!/usr/bin/env python3
"""Replay all dependency patch layers outside the user's checkout."""
from pathlib import Path
import tempfile,subprocess,re,hashlib
r=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='zombie-patch-replay-') as td:
 for module,patches in [('falso_jni',['falso_jni.patch']),('falso_ndk',['falso_ndk.patch']),('vitagl',['vitagl.patch','vitagl_pass10.patch','vitagl_engineering.patch'])]:
  dest=Path(td)/module;dest.mkdir();names=set()
  for patch in patches:
   names.update(re.findall(r'^\+\+\+ b/(.+)$',(r/'patches'/patch).read_text(),re.M))
  for name in names:
   result=subprocess.run(['git','-C',str(r/'lib'/module),'show','HEAD:'+name],capture_output=True)
   if result.returncode==0:
    target=dest/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(result.stdout)
  for patch in patches:
   subprocess.run(['git','apply','--recount','--ignore-space-change',str(r/'patches'/patch)],cwd=dest,check=True)
  for name in names:
   a=(dest/name).read_bytes().replace(b'\r\n',b'\n');b=(r/'lib'/module/name).read_bytes().replace(b'\r\n',b'\n')
   assert a==b,(module,name,hashlib.sha256(a).hexdigest(),hashlib.sha256(b).hexdigest())
  print(module,'patch replay PASS:',len(names),'exact final files')
