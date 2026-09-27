#!/usr/bin/env python3
from pathlib import Path
import hashlib,re,struct
from elftools.elf.elffile import ELFFile
r=Path(__file__).resolve().parent.parent
so=r/'demo/libzombie_shooter.so'
assert hashlib.sha256(so.read_bytes()).hexdigest()=='5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7'
with so.open('rb') as f:
    elf=ELFFile(f);symbols={s.name:s['st_value'] for s in elf.get_section_by_name('.dynsym').iter_symbols()}
    def check(name,offset,words):
        value=symbols[name];assert value&1 and value==offset+1,(name,hex(value))
        seg=next(s for s in elf.iter_segments() if s['p_type']=='PT_LOAD' and s['p_vaddr']<=offset<s['p_vaddr']+s['p_filesz'])
        f.seek(seg['p_offset']+offset-seg['p_vaddr']);actual=f.read(4*len(words));assert actual==struct.pack('<'+'I'*len(words),*words),name
    n=0
    for path in ['source/utils/map_profile.c','source/utils/audio_stream.c']:
        text=(r/path).read_text()
        for name,offset,a,b in re.findall(r'"(_ZN[^"\n]+)",\s*(0x[0-9a-f]+),\s*\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}',text):
            check(name,int(offset,16),[int(a,16),int(b,16)]);n+=1
    check('_ZNK6STRING5c_strEv',0x3dc788,[0x47706800]);check('_ZN5sound10BaseStream4stopEv',0x4d4da8,[0xaf02b5b0,0x4604b0a4])
    assert n==16,n
print('Canonical SO hooks PASS: SHA256, 16 exact symbols/offsets/Thumb/prologues + 2 native helpers; trampoline mismatch fallback covered by existing regression')
