from pathlib import Path
from elftools.elf.elffile import ELFFile
import struct,collections
from unicorn import *
from unicorn.arm_const import *
# Controlled host fixture: the original UID boundary yields a 16-byte ID,
# allocation/libc and single-thread synchronization are host fixtures.
# This executes original crypto instructions; it does not emulate Vita threads.
import hashlib
so=Path(__file__).resolve().parent.parent/'demo/libzombie_shooter.so'
assert hashlib.sha256(so.read_bytes()).hexdigest()=='5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7'
f=so.open('rb');e=ELFFile(f)
u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0,0xb00000);u.mem_map(0xf00000,0x100000);u.mem_map(0x2000000,0x1000000)
for s in e.iter_segments():
 if s['p_type']=='PT_LOAD':u.mem_write(s['p_vaddr'],s.data())
sy=e.get_section_by_name('.dynsym');symbols={s.name:s['st_value'] for s in sy.iter_symbols()};external={};heap=0x2000000;allocations={};calls=collections.Counter()
def word(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def put(a,*values):u.mem_write(a,struct.pack('<'+'I'*len(values),*values))
def alloc(n):
 global heap
 p=heap;heap+=(n+15)&~15;assert heap<0x2f00000;allocations[p]=n;return p
def cstring(a):
 if a==0:return b''
 b=bytes(u.mem_read(a,4096));return b.split(b'\0',1)[0]
def ret(v=None):
 if v is not None:u.reg_write(UC_ARM_REG_R0,v&0xffffffff)
 u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
for section in ['.rel.dyn','.rel.plt']:
 for rel in e.get_section_by_name(section).iter_relocations():
  typ=rel['r_info_type'];a=rel['r_offset'];s=sy.get_symbol(rel['r_info_sym']);value=s['st_value']
  if s['st_shndx']=='SHN_UNDEF':
   if s.name not in external:external[s.name]=0xf01000+16*len(external)
   value=external[s.name]
  if typ==23:continue
  if typ==2:put(a,(word(a)+value)&0xffffffff)
  elif typ in [21,22]:put(a,value)
  elif typ in [0,17,18,19]:pass
  else:raise RuntimeError(('reloc',typ,s.name))
inverse={v:k for k,v in external.items()};pending=[];tls={};tlskey=1;sha_pending=[];sha_verified=[]
for a in inverse:u.mem_write(a,b'\x1e\xff\x2f\xe1')
guard=external['__stack_chk_guard'];put(guard,0x12345678)
def callback(uc,a,size,unused):
 global tlskey
 if sha_pending and a==sha_pending[-1][0]:
  _,dst,expected=sha_pending.pop()
  assert bytes(uc.mem_read(dst,32))==expected,'Original SHA256 differs from independent hashlib'
  sha_verified.append(expected)
 if a==symbols['SHA256']&~1:
  ptr=uc.reg_read(UC_ARM_REG_R0);length=uc.reg_read(UC_ARM_REG_R1);dst=uc.reg_read(UC_ARM_REG_R2)
  sha_pending.append((uc.reg_read(UC_ARM_REG_LR)&~1,dst,hashlib.sha256(bytes(uc.mem_read(ptr,length))).digest()))
 if a==0xf00080:uc.reg_write(UC_ARM_REG_LR,pending.pop());ret(0);return
 if a==symbols['_ZN4core6crypto11CryptEngine14applicationUIDEv']&~1:
  dst=uc.reg_read(UC_ARM_REG_R0);b=b'a1b2c3d4e5f60718';p=alloc(len(b));uc.mem_write(p,b);put(dst,p,p+len(b),p+len(b));ret();return
 if a not in inverse:return
 name=inverse[a];calls[name]+=1
 x,y,z,w=[uc.reg_read(v) for v in [UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3]]
 if name in ['malloc','_Znwj','_Znaj']:ret(alloc(max(x,1)))
 elif name=='calloc':p=alloc(max(x*y,1));uc.mem_write(p,bytes(x*y));ret(p)
 elif name=='realloc':
  p=alloc(max(y,1)); n=min(allocations.get(x,0),y)
  if n:uc.mem_write(p,bytes(uc.mem_read(x,n)))
  ret(p)
 elif name in ['free','_ZdlPv','_ZdaPv','__aeabi_atexit','__cxa_atexit']:ret(0)
 elif name in ['strlen','__strlen_chk']:ret(len(cstring(x)))
 elif name in ['memcpy','memmove','__aeabi_memcpy','__aeabi_memcpy4','__aeabi_memcpy8','__aeabi_memmove','__aeabi_memmove4','__aeabi_memmove8']:
  if z:uc.mem_write(x,bytes(uc.mem_read(y,z)))
  ret(x)
 elif name in ['memset']:uc.mem_write(x,bytes([y&255])*z);ret(x)
 elif name.startswith('__aeabi_memclr'):uc.mem_write(x,bytes(y));ret()
 elif name.startswith('__aeabi_memset'):uc.mem_write(x,bytes([z&255])*y);ret()
 elif name=='strstr':i=cstring(x).find(cstring(y));ret(x+i if i>=0 else 0)
 elif name=='strcmp':ret((cstring(x)>cstring(y))-(cstring(x)<cstring(y)))
 elif name=='memcmp':a1=bytes(uc.mem_read(x,z));a2=bytes(uc.mem_read(y,z));ret((a1>a2)-(a1<a2))
 elif name=='strcpy':uc.mem_write(x,cstring(y)+b'\0');ret(x)
 elif name=='strncmp':ret((cstring(x)[:z]>cstring(y)[:z])-(cstring(x)[:z]<cstring(y)[:z]))
 elif name in ['strchr','strrchr']:
  b=cstring(x)+b'\0';i=b.find(bytes([y&255])) if name=='strchr' else b.rfind(bytes([y&255]));ret(x+i if i>=0 else 0)
 elif name in ['getenv','getauxval']:ret(0)
 elif name in ['fopen','fopen64']:put(0xf00090,2);ret(0)
 elif name=='__vsprintf_chk':
  fmt=cstring(w); va=word(uc.reg_read(UC_ARM_REG_SP)); v=word(va); assert fmt==b'%02X',fmt; b=('%02X'%v).encode(); uc.mem_write(x,b+b'\0'); ret(len(b))
 elif name=='rand':ret(7)
 elif name=='__errno':ret(0xf00090)
 elif name in ['pthread_mutex_lock','pthread_mutex_unlock','pthread_rwlock_rdlock','pthread_rwlock_wrlock','pthread_rwlock_unlock','pthread_rwlock_init','pthread_rwlock_destroy','pthread_mutex_init','pthread_mutex_destroy']:ret(0)
 elif name in ['pthread_cond_init','pthread_cond_destroy','pthread_cond_broadcast','pthread_cond_signal','pthread_cond_wait']:ret(0)
 elif name=='pthread_once':
  if word(x):ret(0)
  else:put(x,1);pending.append(uc.reg_read(UC_ARM_REG_LR));uc.reg_write(UC_ARM_REG_LR,0xf00080);uc.reg_write(UC_ARM_REG_PC,y)
 elif name=='pthread_key_create':put(x,tlskey);tlskey+=1;ret(0)
 elif name=='pthread_getspecific':ret(tls.get(x,0))
 elif name=='pthread_setspecific':tls[x]=y;ret(0)
 elif name=='__cxa_guard_acquire':ret(0 if word(x)&1 else 1)
 elif name=='__cxa_guard_release':put(x,1);ret()
 elif name=='__aeabi_uidivmod':uc.reg_write(UC_ARM_REG_R1,x%y);ret(x//y)
 elif name=='__aeabi_uidiv':ret(x//y)
 else:raise RuntimeError(('external',name,hex(a),hex(uc.reg_read(UC_ARM_REG_LR)),x,y,z,w))
u.hook_add(UC_HOOK_CODE,callback)
u.reg_write(UC_ARM_REG_FPEXC,0x40000000)
def invoke(name,*args):
 u.reg_write(UC_ARM_REG_SP,0x2fff000);u.reg_write(UC_ARM_REG_LR,0xf00000)
 for reg,val in zip([UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3],args):u.reg_write(reg,val)
 u.emu_start(symbols[name],0xf00000,count=20000000)
 assert u.reg_read(UC_ARM_REG_PC)==0xf00000,(name,hex(u.reg_read(UC_ARM_REG_PC)),dict(calls))
 return u.reg_read(UC_ARM_REG_R0)
engine=invoke('_ZN4core6crypto12ICryptEngine8instanceEv');print('engine',hex(engine))
output=alloc(12)
invoke('_ZN4core6crypto11CryptEngine13encryptionKeyEv',output,engine)
start,end,cap=struct.unpack('<3I',u.mem_read(output,12));key=bytes(u.mem_read(start,end-start))
print('NATIVE key vector',hex(start),hex(end),'length=',len(key),'sha256=',__import__('hashlib').sha256(key).hexdigest())
print('external calls',dict(calls))




err=invoke('ERR_peek_last_error');print('openssl last error',hex(err))
if err:
 p=alloc(256);invoke('ERR_error_string_n',err,p,256);print(cstring(p))

value=alloc(12);p=alloc(8);u.mem_write(p,b'example\0');put(value,p,7,8)
out=alloc(12);invoke('_ZN6STRINGC1Ev',out)
r=invoke('_ZN4core6crypto7encryptERK6STRINGRS1_',value,out)
print('encrypt',r,'output words',struct.unpack('<3I',u.mem_read(out,12)))
err=invoke('ERR_peek_last_error');print('openssl last error',hex(err))
if err:
 p=alloc(256);invoke('ERR_error_string_n',err,p,256);print(cstring(p))

plain=alloc(12);invoke('_ZN6STRINGC1Ev',plain)
assert invoke('_ZN4core6crypto7decryptERK6STRINGRS1_RKNSt6__ndk16vectorIhNS5_9allocatorIhEEEE',out,plain,output)==1
ptr,length,capacity=struct.unpack('<3I',u.mem_read(plain,12));assert bytes(u.mem_read(ptr,length))==b'example'
assert len(key)==32 and any(key)
assert len(sha_verified)>=4 and not sha_pending
print('Canonical native crypto PASS: SHA256 independently checked, 32-byte key, original encrypt/decrypt roundtrip; Vita imports remain unverified')


