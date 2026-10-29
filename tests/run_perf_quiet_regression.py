#!/usr/bin/env python3
"""Execute the actual I/O wrapper bodies with deterministic backend/profiler spies."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parent.parent
source=(root/'source/utils/perf.c').read_text()
names=['AAssetManager_open_perf','AAsset_seek_perf','AAsset_read_perf','poll_perf','fread_perf','read_perf']
bodies=[]
for name in names:
    start=source.rfind('\n',0,re.search(r'\b'+name+r'\(',source).start())+1
    end=source.index('\n}',start)+2
    bodies.append(source[start:end])
harness=r'''
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#define NDK_PORT 1
#define USE_SCELIBC_IO 1
typedef int AAsset;
typedef int AAssetManager;
typedef struct {unsigned calls;} IOStats;
typedef struct {unsigned calls,us,max_us,positive,infinite,max_timeout;} WaitStats;
static struct {IOStats asset_open,asset_open_ok,asset_open_fail,asset_seek,asset,file,read;WaitStats all,once;} slots[1];
static int setting_log_mode,lookups,clocks,records,backend,missing;
#define LOAD(p) (*(p))
#define STORE(p,v) (*(p)=(v))
#define INC(p,v) (*(p)+=(v))
static int slot_index(void){lookups++;return 0;}
static uint64_t sceKernelGetProcessTimeWide(void){return ++clocks;}
static void io_record(IOStats *s,int bytes,uint64_t start){(void)s;(void)bytes;(void)start;records++;}
static void record_slow_open(const char *name,int mode,uintptr_t caller,int ok,unsigned us){(void)name;(void)mode;(void)caller;(void)ok;(void)us;}
static int asset_index_missing(const char *name){assert(!strcmp(name,"assets/a.vid"));return missing;}
static void asset_index_record_missing(const char *name,int e){assert(!strcmp(name,"assets/a.vid"));assert(e==ENOENT);}
static AAsset *AAssetManager_open(AAssetManager *mgr,const char *name,int mode){assert(mgr==(void*)1 && mode==3 && !strcmp(name,"assets/a.vid"));backend++;errno=ENOENT;return NULL;}
static off_t AAsset_seek(AAsset *a,off_t off,int whence){assert(a==(void*)2 && off==17 && whence==SEEK_SET);backend++;errno=ESPIPE;return -1;}
static int AAsset_read(AAsset *a,void *b,size_t n){assert(a==(void*)2 && b==(void*)3 && n==41);backend++;errno=EIO;return -1;}
static int ALooper_pollAll(int t,int *f,int *e,void **d){assert(t==23 && !f && !e && !d);backend++;return 7;}
static int ALooper_pollOnce(int t,int *f,int *e,void **d){return ALooper_pollAll(t,f,e,d);}
static size_t sceLibcBridge_fread(void *p,size_t s,size_t n,FILE *f){assert(p==(void*)3 && s==4 && n==5 && f==(void*)6);backend++;errno=EIO;return 3;}
static ssize_t fndk_read(int fd,void *p,size_t n){assert(fd==9 && p==(void*)3 && n==41);backend++;errno=EIO;return -1;}
'''
main=r'''
int main(void){
 for(int mode=0;mode<=3;mode++){
  setting_log_mode=mode;lookups=clocks=records=backend=0;missing=0;
  assert(!AAssetManager_open_perf((void*)1,"assets\\a.vid",3) && errno==ENOENT);
  missing=1;assert(!AAssetManager_open_perf((void*)1,"assets/a.vid",3));
  assert(backend==1);missing=0;
  assert(AAsset_seek_perf((void*)2,17,SEEK_SET)==-1 && errno==ESPIPE);
  assert(AAsset_read_perf((void*)2,(void*)3,41)==-1 && errno==EIO);
  assert(poll_perf(0,23,NULL,NULL,NULL)==7);
  assert(poll_perf(1,23,NULL,NULL,NULL)==7);
  assert(fread_perf((void*)3,4,5,(void*)6)==3 && errno==EIO);
  assert(read_perf(9,(void*)3,41)==-1 && errno==EIO);
  assert(backend==7);
  if(mode<2)assert(!lookups && !clocks && !records);
  else assert(lookups && clocks && records);
 }
 puts("Actual I/O wrappers PASS: backend/arguments/results/errno/negative cache preserved; quiet modes skip all profiling");
}
'''
with tempfile.TemporaryDirectory(prefix='zombie-perf-quiet-') as tmp:
    c=Path(tmp)/'test.c';c.write_text(harness+'\n'.join(bodies)+main)
    for opt in (0,3):
        exe=Path(tmp)/'test'
        subprocess.run(['cc','-std=gnu11',f'-O{opt}','-Wall','-Wextra','-Werror',str(c),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
