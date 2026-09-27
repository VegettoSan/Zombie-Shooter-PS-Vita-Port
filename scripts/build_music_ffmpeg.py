#!/usr/bin/env python3
"""Build minimal AAC+MOV FFmpeg locally; never install into the user's VitaSDK."""
from pathlib import Path
import hashlib,json,os,subprocess,tarfile,urllib.request,sys
ROOT=Path(__file__).resolve().parent.parent
VERSION='8.0.1'
SOURCE_SHA='05ee0b03119b45c0bdb4df654b96802e909e0a752f72e4fe3794f487229e5a41'
SDK=Path('/usr/local/vitasdk')
BASE=ROOT/'build-music-ffmpeg'
INSTALL=BASE/'install'
LIBS=['avformat','avcodec','swresample','avutil']
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    if os.environ.get('VITASDK')!=str(SDK):raise RuntimeError('Requires the functional SoftFP VitaSDK')
    BASE.mkdir(exist_ok=True)
    manifest=BASE/'manifest.json'
    if manifest.exists():
        m=json.loads(manifest.read_text())
        if m.get('source_sha256')==SOURCE_SHA and all((INSTALL/'lib'/('lib'+n+'.a')).exists() and digest(INSTALL/'lib'/('lib'+n+'.a'))==m['libraries'].get(n) for n in LIBS):
            print('Original M4A dependency verified: FFmpeg8.0.1 MOV + AAC, local SoftFP archives');return
    archive=BASE/('ffmpeg-'+VERSION+'.tar.xz')
    if not archive.exists():urllib.request.urlretrieve('https://ffmpeg.org/releases/'+archive.name,archive)
    if digest(archive)!=SOURCE_SHA:raise RuntimeError('FFmpeg source checksum mismatch')
    with tarfile.open(archive) as t:t.extractall(BASE,filter='data')
    src=BASE/('ffmpeg-'+VERSION);work=BASE/'objects';work.mkdir(exist_ok=True)
    env=os.environ.copy();env['PATH']=str(SDK/'bin')+':'+env['PATH']
    args=[str(src/'configure'),'--prefix='+str(INSTALL),'--enable-cross-compile','--cross-prefix='+str(SDK/'bin/arm-vita-eabi-'),'--arch=armv7-a','--cpu=cortex-a9','--target-os=none','--disable-everything','--disable-programs','--disable-doc','--disable-network','--disable-shared','--enable-static','--disable-debug','--disable-autodetect','--disable-runtime-cpudetect','--disable-armv5te','--disable-armv6t2','--enable-small','--enable-decoder=aac','--enable-demuxer=mov','--enable-protocol=file','--enable-swresample','--disable-avdevice','--disable-avfilter','--disable-swscale','--disable-pthreads','--extra-cflags=-O3 -mfloat-abi=softfp -D_BSD_SOURCE','--extra-ldflags=-mfloat-abi=softfp']
    with (BASE/'build.log').open('w') as log:
        for cmd in [args,['make','-j'+str(min(os.cpu_count() or 4,8))],['make','install']]:subprocess.run(cmd,cwd=work,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
    for lib,symbol in [('avformat','ff_mov_demuxer'),('avcodec','ff_aac_decoder')]:
        nm=subprocess.check_output([str(SDK/'bin/arm-vita-eabi-nm'),'--defined-only',str(INSTALL/'lib'/('lib'+lib+'.a'))],text=True,stderr=subprocess.DEVNULL)
        if symbol not in nm:raise RuntimeError('Missing original M4A capability: '+symbol)
    manifest.write_text(json.dumps({'version':VERSION,'source_sha256':SOURCE_SHA,'libraries':{n:digest(INSTALL/'lib'/('lib'+n+'.a')) for n in LIBS},'abi':'softfp','features':['aac_decoder','mov_demuxer']},indent=2)+'\n')
    print('Original M4A dependency built: MOV + AAC, SDK unchanged')
if __name__=='__main__':
    try:main()
    except Exception as exc:print('Music dependency failure:',exc,file=sys.stderr);sys.exit(1)
