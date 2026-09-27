"""Execute original ARM scanlines vs scalar/NEON; isolated tests, not Vita FPS.
Dependencies: unicorn==2.1.4, pyelftools==0.32. Canonical SO never modified.
"""
import argparse,io,random,struct,hashlib
from pathlib import Path
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_HOOK_CODE
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
p=argparse.ArgumentParser();p.add_argument('--repo',required=True);p.add_argument('--neon',required=True);args=p.parse_args()
so=(Path(args.repo)/'demo/libzombie_shooter.so').read_bytes()
assert hashlib.sha256(so).hexdigest()=='5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7'
BASE=0x98000000;RAM=0x20000000;STACK=0x300ff000;STOP=0x40000000
DST=RAM+0x10000
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


original,_=machine();unit,sy=machine(args.neon)
def run(u,entry,n,d):
 for reg,value in [(UC_ARM_REG_R0,n),(UC_ARM_REG_R1,d),(UC_ARM_REG_SP,STACK),(UC_ARM_REG_LR,STOP)]:u.reg_write(reg,value)
 sentinels=[]
 for i,reg in enumerate([UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11]):
  value=0x55110000+i;u.reg_write(reg,value);sentinels.append((reg,value))
 for i in range(8,16):
  reg=globals()['UC_ARM_REG_D'+str(i)];value=0x2211000033110000+i;u.reg_write(reg,value);sentinels.append((reg,value))
 u.emu_start(entry,STOP,count=1000000)
 assert u.reg_read(UC_ARM_REG_PC)==STOP and u.reg_read(UC_ARM_REG_SP)==STACK
 for reg,value in sentinels:assert u.reg_read(reg)==value,('callee-saved',reg)
 return u.reg_read(UC_ARM_REG_R0),u.reg_read(UC_ARM_REG_R1)
def oracle(n,d):
 a=n if n<2**31 else n-2**32;b=d if d<2**31 else d-2**32
 q=abs(a)//abs(b);q=-q if (a<0)!=(b<0) else q
 return q&0xffffffff,(a-q*b)&0xffffffff
rng=random.Random(0x41302a);cases=[]
edge=[0,1,2,127,128,255,256,999,1000,1001,65535,65536,2**31-1,2**31,2**31+1,2**32-2,2**32-1]
for d in edge[1:]:
 for n in edge:cases.append((n,d))
 for n in [d-1,d,d+1]:cases.append((n&0xffffffff,d))
for i in range(1200):cases.append((rng.getrandbits(32),rng.randrange(1,2**32)))
for n,d in cases:
 native=run(original,BASE+0x8b7c68,n,d);expected=oracle(n,d);assert native[0]==expected[0],(n,d,native,expected)
 actual=run(unit,sy['division_hook'],n,d);assert actual[0]==expected[0],(n,d,actual,expected)
 # Repeat to exercise immutable hit or safe original fallback after a collision.
 assert run(unit,sy['division_hook'],n,d)[0]==expected[0]
print('Original internal ARM signed quotient:',len(cases),'cases quotient-identical to original ARM cold/warm,  signs, INT_MIN, INT_MIN/-1, preserved registers')
# Verify actual guards surrounding both calls and helper.
assert so[0x8b7c68:0x8b7c88]==struct.pack('<8I',0xe92d4090,0xe28d7004,0xe0204001,0xe0202fc0,0xe0213fc1,0xe0420fc0,0xe0431fc1,0xeb000002)
assert so[0x413026:0x413032]==struct.pack('<3I',0x9916bf24,0xe61ef0a4,0x7080f5c0)
assert so[0x41320a:0x41321e]==struct.pack('<5I',0x31e7f240,0xd392458e,0x46749916,0xe528f0a4,0xc05cf8dd)
# Install actual emitted Thumb BLs and veneer, then halt exactly at original continuation.
VEN=0x38000
unit.mem_write(VEN,struct.pack('<II',0xf000f8df,sy['division_hook']))
for site in [0x41302a,0x413216]:
 unit.reg_write(UC_ARM_REG_R0,DST);unit.reg_write(UC_ARM_REG_R1,BASE+site);unit.reg_write(UC_ARM_REG_R2,VEN)
 # Too far for this low-address veneer must reject without touching output.
 unit.reg_write(UC_ARM_REG_SP,STACK);unit.reg_write(UC_ARM_REG_LR,STOP);unit.emu_start(sy['branch_test'],STOP);assert unit.reg_read(UC_ARM_REG_R0)==0
unit.mem_map(BASE-0x10000,0x10000);VEN=BASE-0x8000
unit.mem_write(VEN,struct.pack('<II',0xf000f8df,sy['division_hook']))
for site in [0x41302a,0x413216]:
 for target in [VEN,BASE+site+0x1000,BASE+site-0x1000]:
  unit.mem_write(DST,b'\xa5'*4);unit.reg_write(UC_ARM_REG_R0,DST);unit.reg_write(UC_ARM_REG_R1,BASE+site);unit.reg_write(UC_ARM_REG_R2,target)
  unit.reg_write(UC_ARM_REG_SP,STACK);unit.reg_write(UC_ARM_REG_LR,STOP);unit.emu_start(sy['branch_test'],STOP);assert unit.reg_read(UC_ARM_REG_R0)==1
  if target==VEN:unit.mem_write(BASE+site,bytes(unit.mem_read(DST,4)))
unit.ctl_flush_tb()
for site in [0x41302a,0x413216]:
 for n,d in cases[:100]:
  unit.reg_write(UC_ARM_REG_R0,n);unit.reg_write(UC_ARM_REG_R1,d);unit.reg_write(UC_ARM_REG_SP,STACK)
  unit.emu_start((BASE+site)|1,BASE+site+4,count=1000000)
  assert unit.reg_read(UC_ARM_REG_PC)==BASE+site+4 and unit.reg_read(UC_ARM_REG_SP)==STACK
  assert unit.reg_read(UC_ARM_REG_R0)==oracle(n,d)[0]
# First call is final instruction in original ITT CS: execute taken and skipped.
for taken in [False,True]:
 n,d=1234567,1000;unit.mem_write(STACK+88,struct.pack('<I',d));unit.reg_write(UC_ARM_REG_R0,n);unit.reg_write(UC_ARM_REG_R1,4444);unit.reg_write(UC_ARM_REG_SP,STACK)
 unit.reg_write(UC_ARM_REG_CPSR,(unit.reg_read(UC_ARM_REG_CPSR)&~(0xf<<28))|((1<<29) if taken else 0))
 unit.emu_start((BASE+0x413026)|1,BASE+0x41302e,count=1000000)
 assert unit.reg_read(UC_ARM_REG_R0)==(oracle(n,d)[0] if taken else n)
 if not taken:assert unit.reg_read(UC_ARM_REG_R1)==4444
print('Both real patched calls PASS: range rejection, forward/backward BL, Thumb veneer -> compiled ARM, original continuation and conditional ITT CS')
# Quantify warm helper only: instruction count, not cycles/FPS.
for n,d in [(200000,1234),(10000000,10000),(0x80000000,0xfffffc18)]:
 numbers=[]
 unit.mem_write(sy['division_cache'],bytes(512))
 run(unit,sy['division_hook'],n,d)
 for u,entry in [(original,BASE+0x8b7c68),(unit,sy['division_hook'])]:
  tally=[0]
  def tick(uc,address,size,user):tally[0]+=1
  h=u.hook_add(UC_HOOK_CODE,tick);u.ctl_flush_tb();run(u,entry,n,d);u.hook_del(h);numbers.append(tally[0])
 print('ARM instructions full production warm quotient n/d',hex(n),hex(d),':',numbers[0],'->',numbers[1],'(not Vita FPS)')
