#!/usr/bin/env python3
"""Compile actual wrappers and call float-typed mocks through SoftFP ABI."""
from pathlib import Path
import subprocess,tempfile,io,struct
from elftools.elf.elffile import ELFFile
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM
from unicorn.arm_const import *
r=Path(__file__).resolve().parent.parent
text=(r/'source/utils/light_pipeline.c').read_text();a=text.index('static uintptr_t draw_source(');b=text.index('void light_pipeline_install(',a)
source=r'''
#include <stdint.h>
#include <stdbool.h>
uintptr_t originals[3];
uint32_t captured[10],records[3];
uint64_t sceKernelGetProcessTimeWide(void) { return UINT64_C(0x12345678); }
void record(unsigned i,uint64_t start) { if(start==UINT64_C(0x12345678)) records[i]++; }
static uint32_t bits(float v) { union { float f;uint32_t u; } x={.f=v};return x.u; }
__attribute__((noinline)) uintptr_t source_mock(void *self,const void *v,float x,float y,uint32_t c,const void *sp,bool s) {
 captured[0]=(uintptr_t)self;captured[1]=(uintptr_t)v;captured[2]=bits(x);captured[3]=bits(y);captured[4]=c;captured[5]=(uintptr_t)sp;captured[6]=s;return 0x55667788;
}
__attribute__((noinline)) uintptr_t as2_mock(void *self,const void *v,float x,float y,float z,float w,float h,uint32_t c,const void *sp,bool s) {
 captured[0]=(uintptr_t)self;captured[1]=(uintptr_t)v;captured[2]=bits(x);captured[3]=bits(y);captured[4]=bits(z);captured[5]=bits(w);captured[6]=bits(h);captured[7]=c;captured[8]=(uintptr_t)sp;captured[9]=s;return 0x66778899;
}
__attribute__((noinline)) uintptr_t stencil_mock(void *self,const void *v,const void *size,bool s) {
 captured[0]=(uintptr_t)self;captured[1]=(uintptr_t)v;captured[2]=(uintptr_t)size;captured[3]=s;return 0x778899aa;
}
''' +text[a:b]+r'''
uintptr_t wrapper_test(unsigned kind) {
 originals[0]=(uintptr_t)source_mock;originals[1]=(uintptr_t)as2_mock;originals[2]=(uintptr_t)stencil_mock;
 if(kind==0) return draw_source((void *)0x12340000,(void *)0x56780000,0x3f812345,0xc0123456,0xaa112233,(void *)0x13570000,true);
 if(kind==1) return draw_as2((void *)0x12340000,(void *)0x56780000,0x3f812345,0xc0123456,0x80000000,0x7f123456,0x00765432,0xbb334455,(void *)0x24680000,false);
 return draw_stencil((void *)0x12340000,(void *)0x56780000,(void *)0x43210000,true);
}
'''
expected=[[0x12340000,0x56780000,0x3f812345,0xc0123456,0xaa112233,0x13570000,1],
 [0x12340000,0x56780000,0x3f812345,0xc0123456,0x80000000,0x7f123456,0x00765432,0xbb334455,0x24680000,0],
 [0x12340000,0x56780000,0x43210000,1]]
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'unit.c').write_text(source)
 for opt in [0,3]:
  subprocess.run(['/usr/local/vitasdk/bin/arm-vita-eabi-gcc','-O'+str(opt),'-Wall','-Wextra','-Werror','-mfloat-abi=softfp','-mfpu=neon','-ffreestanding','-fno-builtin','-nostdlib',str(p/'unit.c'),'-Wl,-Ttext=0x10000,-e,wrapper_test','-o',str(p/'unit.elf')],check=True)
  e=ELFFile(io.BytesIO((p/'unit.elf').read_bytes()));u=Uc(UC_ARCH_ARM,UC_MODE_ARM)
  u.mem_map(0x1000,0x40000);u.mem_map(0x30000000,0x100000);u.mem_map(0x40000000,0x1000)
  for seg in e.iter_segments():
   if seg['p_type']=='PT_LOAD':u.mem_write(seg['p_vaddr'],seg.data())
  sy={x.name:x['st_value'] for x in e.get_section_by_name('.symtab').iter_symbols()}
  u.reg_write(UC_ARM_REG_C1_C0_2,0xf<<20);u.reg_write(UC_ARM_REG_FPEXC,1<<30)
  for kind in range(3):
   u.reg_write(UC_ARM_REG_R0,kind);u.reg_write(UC_ARM_REG_SP,0x300ff000);u.reg_write(UC_ARM_REG_LR,0x40000000)
   u.emu_start(sy['wrapper_test'],0x40000000,count=1000000)
   got=list(struct.unpack('<'+'I'*len(expected[kind]),bytes(u.mem_read(sy['captured'],4*len(expected[kind])))))
   assert got==expected[kind],(opt,kind,got)
   assert u.reg_read(UC_ARM_REG_R0)==[0x55667788,0x66778899,0x778899aa][kind]
   assert u.reg_read(UC_ARM_REG_SP)==0x300ff000
  assert struct.unpack('<3I',bytes(u.mem_read(sy['records'],12)))==(1,1,1)
  print('Light source/AS2/stencil actual wrappers O'+str(opt)+' PASS: 7/10/4 arguments, SoftFP float bits, Color, bool/stack, original return and timing records')
