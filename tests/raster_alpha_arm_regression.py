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
PAL=RAM+0x3000;DST=RAM+0x10000;Z=RAM+0x14000;SRC=RAM+0x18000
OFFSET=[0x470642,0x4707b8];PROLOGUES=[(8,[0xaf03b5f0,0x0f00e92d]),(12,[0xaf03b5f0,0x0f00e92d,0xc008f8d7])]
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
def run(u,entry,data,syms=None,mode=0,n=32,depth=32768,src=SRC,z=Z,dst=DST,pal=PAL):
 u.mem_write(RAM,bytes(data));u.mem_write(STACK,struct.pack('<II',depth,pal))
 if syms:u.mem_write(syms['test_mode'],struct.pack('<I',mode))
 for reg,value in [(UC_ARM_REG_R0,src),(UC_ARM_REG_R1,z),(UC_ARM_REG_R2,dst),(UC_ARM_REG_R3,n&0xffffffff),(UC_ARM_REG_SP,STACK),(UC_ARM_REG_LR,STOP)]:u.reg_write(reg,value)
 sentinels=[]
 for i,reg in enumerate([UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11]):
  value=0x55110000+i;u.reg_write(reg,value);sentinels.append((reg,value))
 for i in range(8,16):
  reg=globals()['UC_ARM_REG_D'+str(i)];value=0x2211000033110000+i;u.reg_write(reg,value);sentinels.append((reg,value))
 u.emu_start(entry,STOP,count=1000000)
 assert u.reg_read(UC_ARM_REG_PC)==STOP,'did not return'
 assert u.reg_read(UC_ARM_REG_SP)==STACK,'SP changed'
 for reg,value in sentinels:assert u.reg_read(reg)==value,'callee-saved register changed'
 return bytes(u.mem_read(RAM,len(data))),u.reg_read(UC_ARM_REG_R0)
rng=random.Random(9027)
def fixture(n,vis,mode,depth):
 data=bytearray([0xa5]*0x24000)
 def put(addr,fmt,*values):struct.pack_into(fmt,data,addr-RAM,*values)
 for i in range(256):put(PAL+i*4,'<I',([0,1,127,128,254,255][i%6]<<24)|rng.randrange(1<<24))
 for i in range(n):
  put(DST+i*4,'<I',rng.randrange(1<<32))
  if mode:put(SRC+i,'<B',i%256)
  else:put(SRC+i*4,'<I',([0,1,127,128,254,255][i%6]<<24)|rng.randrange(1<<24))
  v=0 if vis=='pass' else 65535 if vis=='fail' else rng.randrange(65536)
  if vis=='mixed' and i%7==0:v=depth
  put(Z+i*2,'<H',v)
 return data
checked=0
for mode in range(2):
 for n in [32,33,39,40,63,64,127,128,255,256,511,1024]:
  for vis in ['pass','fail','mixed']:
   for depth in [0,1,32767,32768,65534,65535]:
    data=fixture(n,vis,mode,depth);expected,_=run(original,(BASE+OFFSET[mode])|1,data,mode=mode,n=n,depth=depth)
    for u,syms,label in [(scalar,ss,'scalar'),(neon,ns,'neon')]:
     actual,ret=run(u,syms['alpha_test'],data,syms,mode,n,depth)
     assert ret==1,(mode,n,label,'rejected')
     assert actual==expected,(mode,n,vis,depth,label,next((i for i,(a,b) in enumerate(zip(actual,expected)) if a!=b),None))
    checked+=1
print('Actual ARM alpha scanlines:',checked,'cases; scalar and NEON byte-identical, Z/source/palette/guards unchanged; callee-saved core/VFP/SP preserved')
for mode in range(2):
 for n in [-1,0,1,31,16385]:
  data=fixture(32,'mixed',mode,32768);actual,ret=run(neon,ns['alpha_test'],data,ns,mode,n);assert ret==0 and actual==bytes(data)
 for changes in [dict(dst=SRC),dict(dst=Z),dict(dst=DST+1),dict(z=Z+1),dict(src=0)]+([dict(dst=PAL),dict(pal=PAL+1),dict(pal=0)] if mode else [dict(src=SRC+1)]):
  data=fixture(64,'mixed',mode,32768);actual,ret=run(neon,ns['alpha_test'],data,ns,mode,64,**changes);assert ret==0 and actual==bytes(data),(mode,changes)
 # Completely rejected blocks never read unmapped source/palette.
 data=fixture(64,'fail',mode,0)
 actual,ret=run(neon,ns['alpha_test'],data,ns,mode,64,0,src=0x50000000,pal=0x60000000);assert ret==1 and actual==bytes(data)
 expected,_=run(original,(BASE+OFFSET[mode])|1,data,mode=mode,n=64,depth=0,src=0x50000000,pal=0x60000000);assert actual==expected
print('Guard/fault-access checks passed: lengths, nulls, alignments, aliases; no access to rejected pixels')
# Test exactly patched SO entry, original early-return, halfword hook, compiled
# dispatcher emitter and real fallback prologue. Original file stays immutable.
unit,us=machine(args.neon)
for mode in range(2):
 length,words=PROLOGUES[mode];entry=BASE+OFFSET[mode]+6;tramp=0x38000+mode*64;dispatch=0x39000+mode*64
 prologue=struct.pack('<'+'I'*len(words),*words);assert so[OFFSET[mode]+6:OFFSET[mode]+6+length]==prologue
 unit.mem_write(tramp,prologue+struct.pack('<II',0xf000f8df,(entry+length)|1))
 unit.reg_write(UC_ARM_REG_R0,dispatch);unit.reg_write(UC_ARM_REG_R1,tramp|1);unit.reg_write(UC_ARM_REG_R2,us['hook_test'])
 unit.reg_write(UC_ARM_REG_SP,STACK);unit.reg_write(UC_ARM_REG_LR,STOP);unit.emu_start(us['dispatch_test'],STOP)
 assert bytes(unit.mem_read(dispatch,20))==struct.pack('<IIIII',0xe3530020,0xb59ff000,0xe59ff000,tramp|1,us['hook_test'])
 unit.mem_write(us['test_original'],struct.pack('<I',tramp|1))
 aligned=entry
 if entry&2:unit.mem_write(entry,struct.pack('<H',0xbf00));aligned+=2
 unit.mem_write(aligned,struct.pack('<II',0xf000f8df,dispatch));unit.ctl_flush_tb()
 for n in [-1,0,1,7,8,31,32,33,127]:
  for changes in [dict(),dict(dst=SRC),dict(dst=Z)]:
   data=fixture(max(1,n),'mixed',mode,32768)
   expected,_=run(original,(BASE+OFFSET[mode])|1,data,mode=mode,n=n,**changes)
   actual,_=run(unit,(BASE+OFFSET[mode])|1,data,us,mode,n,**changes);assert actual==expected,('patched entry',mode,n,changes)
print('54 patched-entry cases: original count early return, actual emitted dispatch, stack forwarding, ARM/Thumb interworking, fallback aliases and halfword prologue passed')
for mode in range(2):
 for vis in ['pass','mixed','fail']:
  data=fixture(128,vis,mode,32768);numbers=[]
  for u,entry,syms in [(original,(BASE+OFFSET[mode])|1,None),(neon,ns['alpha_test'],ns)]:
   tally=[0]
   def tick(uc,address,size,user):tally[0]+=1
   h=u.hook_add(UC_HOOK_CODE,tick);u.ctl_flush_tb();run(u,entry,data,syms,mode,128);u.hook_del(h);numbers.append(tally[0])
  print('ARM kernel instructions mode',mode,'visibility',vis,':',numbers[0],'->',numbers[1],'(not Vita cycles/FPS)')
