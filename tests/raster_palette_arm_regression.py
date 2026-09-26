"""Compare actual original ARM palette functions with compiled scalar/NEON code.
Dependencies: unicorn==2.1.4, pyelftools==0.32. This is a unit test, not Vita runtime validation.
"""
import argparse,io,random,struct,hashlib
from pathlib import Path
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_HOOK_CODE
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
p=argparse.ArgumentParser();p.add_argument('--repo',required=True);p.add_argument('--scalar',required=True);p.add_argument('--neon',required=True);args=p.parse_args()
repo=Path(args.repo);so=(repo/'demo/libzombie_shooter.so').read_bytes()
assert hashlib.sha256(so).hexdigest()=='5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7'
BASE=0x98000000;RAM=0x20000000;STACK=0x300ff000;STOP=0x40000000
REF=RAM+0x2000;PAL=RAM+0x3000;DST=RAM+0x10000;DZ=RAM+0x14000;SZ=RAM+0x18000;SRC=RAM+0x1c000
OFFSET=[0x51c3f2,0x51c334,0x51c588,0x51c4d4,0x51c6ac,0x51c64e,0x51c76c,0x51c716]
PROLOGUES=[(12,[0xaf03b5f0,0x0f00e92d,0x6978b084]),(8,[0xaf03b5f0,0x0f00e92d]),(8,[0xaf03b5f0,0x0f00e92d]),(8,[0xaf03b5f0,0x0f00e92d]),(8,[0xaf03b5f0,0x0b00e92d]),(12,[0xaf03b5f0,0x0b00e92d,0xc014f8d7]),(8,[0xaf03b5f0,0x8d04f84d]),(12,[0xaf03b5f0,0x8d04f84d,0xc014f8d7])]
def machine(elfpath=None):
 u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(RAM,0x100000);u.mem_map(0x30000000,0x100000);u.mem_map(STOP,0x1000);u.mem_map(0x1000,0x40000)
 u.reg_write(UC_ARM_REG_C1_C0_2,0xf<<20);u.reg_write(UC_ARM_REG_FPEXC,1<<30)
 if elfpath:
  e=ELFFile(io.BytesIO(Path(elfpath).read_bytes()))
  for seg in e.iter_segments():
   if seg['p_type']=='PT_LOAD':u.mem_write(seg['p_vaddr'],seg.data())
  syms={x.name:x['st_value'] for x in e.get_section_by_name('.symtab').iter_symbols()}
  return u,syms['palette_test'],syms['test_mode']
 u.mem_map(BASE,(len(so)+4095)&~4095);u.mem_write(BASE,so);return u
original=machine();scalar,scalar_entry,scalar_mode=machine(args.scalar);neon,neon_entry,neon_mode=machine(args.neon)
def run(u,entry,data,mode_addr=None,mode=0,count=8,step=1,refs=None,arm=False):
 u.mem_write(RAM,bytes(data));u.mem_write(STACK,struct.pack('<IIII',REF+32,REF+48,step&0xffffffff,count&0xffffffff))
 if mode_addr is not None:u.mem_write(mode_addr,struct.pack('<I',mode))
 for reg,value in [(UC_ARM_REG_R0,RAM+0x1000),(UC_ARM_REG_R1,REF),(UC_ARM_REG_R2,PAL),(UC_ARM_REG_R3,REF+16),(UC_ARM_REG_SP,STACK),(UC_ARM_REG_LR,STOP)]:u.reg_write(reg,value)
 sentinels=[]
 for i,reg in enumerate([UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11]):
  value=0x55110000+i;u.reg_write(reg,value);sentinels.append((reg,value))
 for i in range(8,16):
  reg=globals()['UC_ARM_REG_D'+str(i)];value=0x2211000033110000+i;u.reg_write(reg,value);sentinels.append((reg,value))
 u.emu_start(entry if arm else entry|1,STOP,count=1000000)
 assert u.reg_read(UC_ARM_REG_PC)==STOP,'did not return'
 assert u.reg_read(UC_ARM_REG_SP)==STACK,'stack pointer changed'
 for reg,value in sentinels:assert u.reg_read(reg)==value,'callee-saved register changed'
 return bytes(u.mem_read(RAM,len(data))),u.reg_read(UC_ARM_REG_R0)
rng=random.Random(9026)
def fixture(n,vis):
 data=bytearray([0xA5]*0x24000)
 def put(addr,fmt,*v):struct.pack_into(fmt,data,addr-RAM,*v)
 for addr,ptr in [(REF,DST),(REF+16,DZ),(REF+32,SZ),(REF+48,SRC)]:put(addr,'<I',ptr)
 put(RAM+0x1000+1188,'<i',32768)
 for i in range(256):
  alpha=[0,1,127,128,254,255][i%6];put(PAL+i*4,'<I',(alpha<<24)|rng.randrange(1<<24))
 for i in range(n):
  put(DST+i*4,'<I',rng.randrange(1<<32))
  if vis=='pass':a,b=65535,0
  elif vis=='fail':a,b=0,65535
  else:a,b=rng.randrange(65536),rng.randrange(65536)
  if i%7==0 and vis=='mixed':a=b=32768
  put(DZ+i*2,'<H',b);put(SZ+i*2,'<H',a);put(SRC+i,'<B',rng.randrange(256))
 return data
checked=0
for mode in range(8):
 for n in [8,9,15,16,17,31,32,33,64,127,256]:
  for vis in ['pass','fail','mixed']:
   for repeat in range(3):
    data=fixture(n,vis)
    # Boundary flat depths include 0,32768,65535; signed checks match the original.
    if mode&1:struct.pack_into('<i',data,0x1000+1188,[0,32768,65535][repeat])
    expected,_=run(original,BASE+OFFSET[mode],data,mode=mode,count=n)
    for u,entry,mode_addr,label in [(scalar,scalar_entry,scalar_mode,'scalar'),(neon,neon_entry,neon_mode,'neon')]:
     actual,ret=run(u,entry,data,mode_addr,mode,n)
     assert ret==1,(mode,n,label,'fast path rejected')
     assert actual==expected,(mode,n,vis,label,'byte mismatch',next((i for i,(a,b) in enumerate(zip(actual,expected)) if a!=b),None))
    checked+=1
 # Execute every original fallback trampoline (including halfword aligned entries).
 length,words=PROLOGUES[mode];expected_bytes=struct.pack('<'+'I'*len(words),*words)
 assert so[OFFSET[mode]:OFFSET[mode]+length]==expected_bytes
 trampoline=0x38000+mode*64
 original.mem_write(trampoline,expected_bytes+struct.pack('<II',0xf000f8df,(BASE+OFFSET[mode]+length)|1))
 data=fixture(33,'mixed');expected,_=run(original,BASE+OFFSET[mode],data,mode=mode,count=33)
 actual,_=run(original,trampoline,data,mode=mode,count=33);assert actual==expected,('trampoline',mode)
print('Actual original ARM vs optimized scalar/NEON:',checked,'cases, 8 modes; pixels/depth/refs/guards and callee-saved core/VFP registers identical')
# Conservative rejections must leave the entire guest state untouched.
for mode in range(8):
 for n,step in [(0,1),(-1,1),(7,1),(32,2),(32,-1),(16385,1)]:
  data=fixture(32,'mixed');actual,ret=run(neon,neon_entry,data,neon_mode,mode,n,step);assert ret==0 and actual==bytes(data)
 for kind in ['shared_z','palette_in_destination','shared_ref','unaligned_destination','depth_alias']:
  data=fixture(32,'mixed')
  if kind=='shared_z':
   if mode&1:continue
   struct.pack_into('<I',data,REF+32-RAM,DZ)
  if kind=='palette_in_destination':struct.pack_into('<I',data,REF-RAM,PAL)
  if kind=='shared_ref':
   # Point the destination array at its reference cell.
   struct.pack_into('<I',data,REF-RAM,REF)
  if kind=='unaligned_destination':struct.pack_into('<I',data,REF-RAM,DST+1)
  if kind=='depth_alias':
   if not(mode&1):continue
   struct.pack_into('<I',data,REF-RAM,RAM+0x1000+1188)
  actual,ret=run(neon,neon_entry,data,neon_mode,mode,32)
  if kind=='shared_z' and mode&2:
   expected,_=run(original,BASE+OFFSET[mode],data,mode=mode,count=32);assert ret==1 and actual==expected
  else:assert ret==0 and actual==bytes(data),(mode,kind)
 if mode&1:
  for flat in [-1,65536]:
   data=fixture(32,'mixed');struct.pack_into('<i',data,0x1000+1188,flat)
   actual,ret=run(neon,neon_entry,data,neon_mode,mode,32);assert ret==0 and actual==bytes(data)
print('Guard regression passed: short/negative/huge lengths, strides, aliases, alignment, signed flat depth; all 8 original trampolines return identical state')


for n in [8,32,128,256]:
 for vis in ['pass','mixed','fail']:
  numbers=[]
  for mode in [0,1,2,3,4,5,6,7]:
   data=fixture(n,vis);counts=[]
   for u,entry,mode_addr in [(original,BASE+OFFSET[mode],None),(neon,neon_entry,neon_mode)]:
    tally=[0]
    def tick(uc,address,size,user):tally[0]+=1
    hook=u.hook_add(UC_HOOK_CODE,tick);u.ctl_flush_tb()
    run(u,entry,data,mode_addr,mode,n);u.hook_del(hook);counts.append(tally[0])
   numbers.append(str(mode)+':'+str(counts[0])+'->'+str(counts[1]))
  print('ARM instructions n='+str(n)+' visibility='+vis+' '+' '.join(numbers))



unit,unit_entry,unit_mode=machine(args.neon)
unit.mem_map(BASE,(len(so)+4095)&~4095);unit.mem_write(BASE,so)
e=ELFFile(io.BytesIO(Path(args.neon).read_bytes()))
emit_symbol=next(x['st_value'] for x in e.get_section_by_name('.symtab').iter_symbols() if x.name=='dispatch_test')
for mode in range(8):
 length,words=PROLOGUES[mode];tramp=0x38000+mode*64;dispatch=0x39000+mode*64
 unit.mem_write(tramp,struct.pack('<'+'I'*len(words),*words)+struct.pack('<II',0xf000f8df,(BASE+OFFSET[mode]+length)|1))
 unit.reg_write(UC_ARM_REG_R0,dispatch);unit.reg_write(UC_ARM_REG_R1,tramp|1);unit.reg_write(UC_ARM_REG_R2,unit_entry)
 unit.reg_write(UC_ARM_REG_SP,STACK);unit.reg_write(UC_ARM_REG_LR,STOP);unit.emu_start(emit_symbol|1,STOP)
 assert bytes(unit.mem_read(dispatch,24))==struct.pack('<IIIIII',0xe59dc00c,0xe35c0020,0xb59ff000,0xe59ff000,tramp|1,unit_entry)
 for n in [-1,0,1,7,8,31,32,33,127]:
  data=fixture(max(1,n),'mixed');expected,_=run(original,BASE+OFFSET[mode],data,mode=mode,count=n)
  actual,_=run(unit,dispatch,data,unit_mode,mode,n,arm=True);assert actual==expected,('ARM dispatch',mode,n)
print('Actual emitted ARM dispatch passed: 8 modes, 72 short/long rows; stack argument forwarding and ARM/Thumb interworking preserved')
