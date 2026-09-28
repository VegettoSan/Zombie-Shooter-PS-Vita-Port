#!/usr/bin/env python3
from pathlib import Path
import tempfile,subprocess,hashlib,re,struct
from elftools.elf.elffile import ELFFile
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_LR
r=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='zombie-engineering-tests-') as tmp:
 p=Path(tmp)
 for opt in [0,3]:
  exe=p/'prefs';root=p/str(opt);root.mkdir()
  subprocess.run(['cc','-std=gnu11',f'-O{opt}','-Wall','-Wextra','-Wno-unused-function',str(r/'tests/preferences_restart_regression.c'),'-o',str(exe)],check=True)
  def run(mode,*args):subprocess.run([str(exe),str(root),mode,*args],check=True)
  run('write','opaque-base64-fixture==');run('read','opaque-base64-fixture==')
  run('write','second-fixture==');run('read','second-fixture==')
  run('fail');run('read','second-fixture==')
  # A failed restore leaves a complete backup, recoverable by a new process.
  run('fail','restore-too');run('read','second-fixture==')
  run('write','third-fixture==');run('read','third-fixture==')
  (root/'shared_preferences.tmp').write_bytes(b'interrupted-write')
  run('read','third-fixture==')
  (root/'shared_preferences.bin').write_bytes(b'truncated')
  run('read','second-fixture==')
  run('write','recovered-fixture==');run('read','recovered-fixture==')
  exe=p/'upload';subprocess.run(['cc','-std=gnu11',f'-O{opt}','-Wall','-Wextra','-Werror',str(r/'tests/upload_reuse_guard_regression.c'),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
so=r/'demo/libzombie_shooter.so';assert hashlib.sha256(so.read_bytes()).hexdigest()=='5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7'
with so.open('rb') as f:
 elf=ELFFile(f);uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
 uc.mem_map(0,0xa00000)
 for seg in elf.iter_segments():
  if seg['p_type']=='PT_LOAD':uc.mem_write(seg['p_vaddr'],seg.data())
 syms={s.name:s['st_value'] for s in elf.get_section_by_name('.dynsym').iter_symbols()}
 text=(r/'source/contract_trace.inc').read_text()
 for name,off,a,b in re.findall(r'\{"([^"]+)",0x([0-9a-f]+),\{0x([0-9a-f]+),0x([0-9a-f]+)\}',text):
  address=int(off,16);assert syms[name]==address|1
  assert bytes(uc.mem_read(address,8))==struct.pack('<II',int(a,16),int(b,16)),name
 # Execute canonical AndroidKeyboardControl::handleKey, not a C reimplementation.
 accepted=[]
 for code in range(0,256):
  uc.reg_write(UC_ARM_REG_R0,0);uc.reg_write(UC_ARM_REG_R1,code);uc.reg_write(UC_ARM_REG_LR,0x980001)
  uc.emu_start(0x3d0579,0x980000,count=100)
  if uc.reg_read(UC_ARM_REG_R0):accepted.append(code)
 for code in [4,19,20,21,22,96,97,99,100,102,103,104,105,106,107,108,109]:assert code in accepted,code
 print('Canonical ARM handleKey accepts:',accepted)
print('Engineering regressions PASS O0/O3, independent process durability, canonical ARM filter and guarded trace prologues')
