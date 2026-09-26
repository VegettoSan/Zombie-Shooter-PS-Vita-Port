#!/usr/bin/env python3
"""Decode original AAC tracks to PCM sidecars; preserve rate/channels and sources.
Requires ffmpeg. Game-owned music stays local and must not be committed.
"""
import argparse, hashlib, json, subprocess, zipfile
from pathlib import Path
NAMES=('menu_mus01','mus01','mus02','amb01','rain')
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--data',type=Path,default=Path('data'))
    p.add_argument('--ffmpeg',default='ffmpeg')
    p.add_argument('--output',type=Path,default=Path('build-session-debug/music-pcm.zip'))
    a=p.parse_args(); records=[]
    music=a.data.resolve()/'assets/music'
    for name in NAMES:
        src=music/(name+'.m4a'); dst=music/(name+'.wav'); temp=music/(name+'.decode.tmp.wav')
        if not src.is_file(): p.error('Missing original: '+str(src))
        before=hashlib.sha256(src.read_bytes()).hexdigest()
        subprocess.run([a.ffmpeg,'-v','error','-nostdin','-y','-i',str(src),'-map','0:a:0',
                        '-c:a','pcm_f32le',str(temp)],check=True)
        # Decode original and sidecar back to float PCM; require sample identity.
        def pcm(path):
            return subprocess.check_output([a.ffmpeg,'-v','error','-nostdin','-i',str(path),
                                            '-map','0:a:0','-f','f32le','-c:a','pcm_f32le','-'])
        original=pcm(src); converted=pcm(temp)
        if original != converted: raise RuntimeError('PCM mismatch: '+name)
        assert hashlib.sha256(src.read_bytes()).hexdigest()==before
        temp.replace(dst)
        records.append(dict(track=name,source_sha256=before,pcm_sha256=hashlib.sha256(original).hexdigest(),pcm_bytes=len(original)))
    a.output.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(a.output,'w',compression=zipfile.ZIP_DEFLATED) as z:
        for name in NAMES: z.write(music/(name+'.wav'),'zombieshooter/assets/music/'+name+'.wav')
        z.writestr('zombieshooter/music-pcm-manifest.json',json.dumps(records,indent=2))
    print(json.dumps(dict(package=str(a.output.resolve()),tracks=records),indent=2))
if __name__=='__main__': main()
