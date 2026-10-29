#!/usr/bin/env python3
"""Validate exact target instructions and relocation against the real 3.6.1 ELF."""
import hashlib, re, struct, subprocess, sys, tempfile
from pathlib import Path
from elftools.elf.elffile import ELFFile
root=Path(__file__).resolve().parent.parent
so=Path(sys.argv[1])
assert hashlib.sha256(so.read_bytes()).hexdigest()=='cb461ac47de79536824e8f7b64fa82293b796bcc159675b4f5ad6304e60bdca1'
with so.open('rb') as f, tempfile.TemporaryDirectory(prefix='zombie-361-frame-') as tmp:
    elf=ELFFile(f)
    symbols={s.name:s['st_value'] for s in elf.get_section_by_name('.dynsym').iter_symbols()}
    def data(offset,n):
        seg=next(s for s in elf.iter_segments() if s['p_type']=='PT_LOAD' and s['p_vaddr']<=offset<s['p_vaddr']+s['p_filesz'])
        f.seek(seg['p_offset']+offset-seg['p_vaddr']);return f.read(n)
    assert symbols['_ZN8OpenGLES6UnLockEb']==0x47ee39
    # Verify call target is the PLT slot for glTexSubImage2D, not another GL call.
    rels=elf.get_section_by_name('.rel.plt')
    ds=elf.get_section(rels['sh_link'])
    idx=next(i for i,r in enumerate(rels.iter_relocations()) if ds.get_symbol(r['r_info_sym']).name=='glTexSubImage2D')
    assert elf.get_section_by_name('.plt')['sh_addr']+32+idx*16==0x8ea370
    for path in ['source/patch.c','source/utils/audio_stream.c']:
        text=(root/path).read_text()
        for name,off,a,b in re.findall(r'\{"(_Z[^"\n]+)",(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}',text):
            if path.endswith('patch.c') and name not in ['_ZN5GRAPH4TactEi','_ZN5GRAPH12softwareTactEi','_ZN3MAP4tactEv','_ZN8OpenGLES7preTactEv','_ZN8OpenGLES8PostTactEi','_ZNK16SPRITE_COLLECTOR9DrawLayerEiRK7VECTOR2S2_bb']:continue
            offset=int(off,16)
            assert symbols[name]==offset|1,name
            assert data(offset,8)==struct.pack('<II',int(a,16),int(b,16)),name
    assert symbols['_ZN12scale_config15calculateFactorERKNS_19ScreenConfigurationE']==0x4959c1
    assert data(0x4959c0,8)==struct.pack('<II',0xaf03b5f0,0xbd04f84d)
    assert symbols['_ZN12scale_config6factorEv']==0x495de1
    assert symbols['_ZN12scale_config9setFactorEf']==0x495ded
    fixture=Path(tmp)/'unlock.bin';fixture.write_bytes(data(0x47ee38,0x94))
    for opt in (0,3):
        exe=Path(tmp)/'test'
        subprocess.run(['cc','-std=gnu11',f'-O{opt}','-Wall','-Wextra','-Werror',str(root/'tests/software_framebuffer_contract_regression.c'),'-o',str(exe)],check=True)
        subprocess.run([str(exe),str(fixture)],check=True)
print('Exact 1161 SHA256, framebuffer PLT target, active engine/music/scale guards: PASS')
