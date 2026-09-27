"""Execute original ARM scanlines vs scalar/NEON; isolated tests, not Vita FPS.
Dependencies: unicorn==2.1.4, pyelftools==0.32. Canonical SO never modified.
"""
import argparse,io,random,struct,hashlib
from pathlib import Path
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_HOOK_CODE
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
p=argparse.ArgumentParser();p.add_argument('--repo',required=True);p.add_argument('--scalar',required=True);p.add_argument('--neon',required=True);args=p.parse_args()
so=(Path(args.repo)/'demo/libzombie_shooter.so').read_bytes()
assert hashlib.sha256(so).hexdigest()=='5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7'
BASE=0x98000000;RAM=0x20000000;STACK=0x300ff000;STOP=0x40000000
DST=RAM+0x10000;Z=RAM+0x14000;SRC=RAM+0x18000;A=RAM+0x20000
def machine(elfpath=None):
 u=Uc(UC_ARCH_ARM,UC_MODE_ARM)
 for addr,size in [(RAM,0x100000),(0x30000000,0x100000),(STOP,0x1000),(0x1000,0x40000),(BASE,(len(so)+4095)&~4095)]:u.mem_map(addr,size)
 u.mem_write(BASE,so);u.reg_write(UC_ARM_REG_C1_C0_2,0xf<<20);u.reg_write(UC_ARM_REG_FPEXC,1<<30)
 syms={}
 if elfpath:
  e=ELFFile(io.BytesIO(Path(elfpath).read_bytes()))
  for seg in e.iter_segments():
   if seg['p_type']=='PT_LOAD':u.mem_write(seg['p_vaddr'],seg.data())
  syms={x.name:x['st_value'] for x in e.get_section_by_name('.symtab').iter_symbols()}
 return u,syms

original,_=machine();scalar,ss=machine(args.scalar);neon,ns=machine(args.neon)
def run(u,entry,data,n=32,depth=0,a=A,s=SRC,z=Z,d=DST):
 u.mem_write(RAM,bytes(data));u.mem_write(STACK,struct.pack('<II',n&0xffffffff,depth))
 for reg,value in [(UC_ARM_REG_R0,a),(UC_ARM_REG_R1,s),(UC_ARM_REG_R2,z),(UC_ARM_REG_R3,d),(UC_ARM_REG_SP,STACK),(UC_ARM_REG_LR,STOP)]:u.reg_write(reg,value)
 sentinels=[]
 for i,reg in enumerate([UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11]):
  value=0x55110000+i;u.reg_write(reg,value);sentinels.append((reg,value))
 for i in range(8,16):
  reg=globals()['UC_ARM_REG_D'+str(i)];value=0x2211000033110000+i;u.reg_write(reg,value);sentinels.append((reg,value))
 u.emu_start(entry,STOP,count=1000000)
 assert u.reg_read(UC_ARM_REG_PC)==STOP and u.reg_read(UC_ARM_REG_SP)==STACK
 for reg,value in sentinels:assert u.reg_read(reg)==value,('callee-saved',reg)
 return bytes(u.mem_read(RAM,len(data))),u.reg_read(UC_ARM_REG_R0)
rng=random.Random(51000)
def fixture(n,depth,kind):
 data=bytearray([0xa5]*0x24000)
 signed=depth if depth<32768 else depth-65536
 edges=[-32768,-32767,-129,-128,-1,0,1,7,8,15,16,119,120,127,128,129,32766,32767]
 for i in range(n):
  a=rng.randrange(-32768,32768);z=rng.randrange(-32768,32768)
  if kind=='edge':
   z=0;a=max(-32768,min(32767,edges[i%len(edges)]-signed))
  elif kind=='fail':a=-32768;z=32767
  struct.pack_into('<h',data,A-RAM+i*2,a);struct.pack_into('<h',data,Z-RAM+i*2,z)
  struct.pack_into('<H',data,SRC-RAM+i*2,rng.randrange(65536))
 return data
checked=0
for n in [16,17,23,24,31,32,63,127,128,255,511,1024]:
 for depth in [0,1,127,128,32767,32768,65534,65535]:
  for kind in ['random','edge','fail']:
   data=fixture(n,depth,kind);expected,_=run(original,(BASE+0x510b00)|1,data,n,depth)
   for u,syms,label in [(scalar,ss,'scalar'),(neon,ns,'NEON')]:
    actual,ret=run(u,syms['light_test'],data,n,depth)
    assert ret==1 and actual==expected,(n,depth,kind,label,next((i for i,(a,b) in enumerate(zip(actual,expected)) if a!=b),None))
   checked+=1
print('Light kernel:',checked,'original ARM vs scalar/NEON byte-identical; signed 32-bit delta, 127/128 threshold, tails, guard bytes, callee-saved registers')
for n in [-1,0,1,15,16385]:
 data=fixture(32,0,'random');actual,ret=run(neon,ns['light_test'],data,n);assert ret==0 and actual==bytes(data)
for changes in [dict(d=SRC),dict(d=A),dict(d=Z),dict(d=SRC+2),dict(d=A-2),dict(d=DST+1),dict(a=A+1),dict(s=SRC+1),dict(z=Z+1),dict(a=0)]:
 data=fixture(64,0,'random');actual,ret=run(neon,ns['light_test'],data,64,**changes);assert ret==0 and actual==bytes(data),changes
# Exact trampoline/hook at aligned Thumb entry, real stack args/fallback.
unit,us=machine(args.neon);entry=BASE+0x510b00;tramp=0x38000
prologue=struct.pack('<II',0xaf03b5f0,0x0b00e92d);assert so[0x510b00:0x510b08]==prologue
unit.mem_write(tramp,prologue+struct.pack('<II',0xf000f8df,(entry+8)|1));unit.mem_write(us['test_original'],struct.pack('<I',tramp|1))
unit.mem_write(entry,struct.pack('<II',0xf000f8df,us['hook_test']));unit.ctl_flush_tb()
for n in [-1,0,1,7,15,16,17,63,128]:
 for changes in [dict(),dict(d=SRC),dict(d=A),dict(d=Z),dict(d=SRC+2),dict(d=A-2),dict(d=DST+1)]:
  data=fixture(max(1,n),65535,'random');expected,_=run(original,entry|1,data,n,65535,**changes)
  actual,_=run(unit,entry|1,data,n,65535,**changes);assert actual==expected,('hook',n,changes)
print('63 patched-entry tests PASS: 6 args, original fallback for short/misaligned/overlapping rows, SP and ARM/Thumb preserved')
for kind in ['edge','random','fail']:
 data=fixture(128,0,kind);numbers=[]
 for u,entry in [(original,(BASE+0x510b00)|1),(neon,ns['light_test'])]:
  tally=[0]
  def tick(uc,address,size,user):tally[0]+=1
  h=u.hook_add(UC_HOOK_CODE,tick);u.ctl_flush_tb();run(u,entry,data,128);u.hook_del(h);numbers.append(tally[0])
 print('128 pixels',kind,'ARM instructions:',numbers[0],'->',numbers[1],'(not Vita cycles/FPS)')
# In fully rejected blocks original must never access source color.
data=fixture(32,0,'fail')
for u,entry in [(original,(BASE+0x510b00)|1),(neon,ns['light_test'])]:
 actual,_=run(u,entry,data,32,s=0x50000000)
print('Rejected-block source access test PASS')
