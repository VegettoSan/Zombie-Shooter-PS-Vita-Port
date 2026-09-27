#!/usr/bin/env python3
from pathlib import Path
import tempfile,subprocess,shlex,hashlib
r=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='zombie-stability-') as td:
    p=Path(td);(p/'assets').mkdir();(p/'bad.m4a').write_bytes(b'invalid mp4')
    queue_source=(r/'lib/opensles_clear/IBufferQueue.c').read_text()
    queue_source=queue_source.replace('#include "sles_allinclusive.h"','')
    for header in ['utils/logger.h','utils/perf.h','psp2/kernel/processmgr.h']:
        queue_source=queue_source.replace('#include "'+header+'"','').replace('#include <'+header+'>','')
    (p/'queue-under-test.c').write_text(queue_source)
    for opt in [0,3]:
        reuse=p/'reuse'
        subprocess.run(['cc','-std=gnu11',f'-O{opt}','tests/render_reuse_policy_regression.c','-o',str(reuse)],cwd=r,check=True)
        subprocess.run([str(reuse)],check=True)
        queue=p/'queue' 
        subprocess.run(['cc','-std=gnu11',f'-O{opt}','-pthread','-Itests','-Ilib/opensles_clear','-I'+td,'tests/opensles_queue_regression.c','lib/opensles_clear/MixerGate.c','-o',str(queue)],cwd=r,check=True)
        subprocess.run([str(queue)],check=True)
        obj=p/'cache.o';exe=p/'cache'
        subprocess.run(['cc','-std=gnu11',f'-O{opt}','-Wall','-Wextra','-Werror','-pthread','-Isource','-c','source/utils/asset_cache.c','-o',str(obj)],cwd=r,check=True)
        subprocess.run(['c++','-std=c++20',f'-O{opt}','-pthread','-Isource','-Ilib/falso_ndk',f'-DDATA_PATH="{td}/"','tests/asset_cache_regression.cpp',str(obj),'-o',str(exe)],cwd=r,check=True)
        subprocess.run([str(exe)],check=True)
        exe=p/'decoder';flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','libavformat','libavcodec','libswresample','libavutil'],text=True))
        subprocess.run(['cc','-std=gnu11',f'-O{opt}','-Wall','-Wextra','-Werror','-Isource','tests/music_decoder_regression.c','source/utils/music_decoder.c','-o',str(exe)]+flags,cwd=r,check=True)
        worker=p/'worker'
        subprocess.run(['cc','-std=gnu11',f'-O{opt}','-pthread','-Isource',f'-DDATA_PATH="{r}/data/"','tests/music_worker_regression.c','source/utils/music_decoder.c','-o',str(worker)]+flags,cwd=r,check=True)
        subprocess.run([str(worker)],check=True)
        for src in sorted((r/'data/assets/music').glob('*.m4a')):
            out=p/'out.raw';reference=p/'reference.raw'
            subprocess.run([str(exe),str(src),str(out),str(p/'bad.m4a')],check=True)
            subprocess.run(['ffmpeg','-v','error','-nostdin','-y','-i',str(src),'-f','s16le','-ac','2','-ar','44100',str(reference)],check=True)
            a=out.read_bytes();b=reference.read_bytes();assert a==b,(src.name,len(a),len(b))
            print('PCM byte-exact vs ffmpeg',src.name,hashlib.sha256(a).hexdigest())
print('Stability regressions PASS O0/O3')
