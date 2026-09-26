/* Compile the actual index against a fault-injectable directory backend. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <stdarg.h>
#define DATA_PATH "test:/game/"
#define ZOMBIE_ASSET_INDEX_HOST 1
typedef struct { char d_name[256]; } SceIoDirent;
static unsigned opens,reads,closes,position;
static int fault,fail_alloc,generated;
static const char *files[]={"020.vid","Game_Logo.PNG","2503.png",".hidden",NULL};
static int sceIoDopen(const char *path) {
    assert(strstr(path,DATA_PATH "assets/")==path);opens++;position=0;
    return fault==1?-1:17;
}
static int sceIoDread(int fd,SceIoDirent *entry) {
    assert(fd==17);reads++;
    if(fault==2 && position==2) return -1;
    if(generated) {
        if(position==(unsigned)generated) return 0;
        snprintf(entry->d_name,sizeof(entry->d_name),"image_%u.png",position++);return 1;
    }
    const char *name=files[position];if(!name) return 0;
    if(fault==4 && position==2) name="bad name.png";
    strcpy(entry->d_name,name);position++;return 1;
}
static int sceIoDclose(int fd) { assert(fd==17);closes++;return fault==3?-1:0; }
static uint64_t sceKernelGetProcessTimeWide(void) { static uint64_t time;return time+=100; }
static void *checked_realloc(void *ptr,size_t n) { return fail_alloc?NULL:realloc(ptr,n); }
#define ASSET_INDEX_REALLOC checked_realloc
static void l_perf(const char *fmt,...) { (void)fmt; }
#include "utils/asset_index.c"
static void reset(void) {
    for(unsigned i=0;i<3;++i) {
        free(directories[i].hashes);directories[i].hashes=NULL;
        directories[i].count=directories[i].capacity=0;directories[i].state=0;
    }
    memset(&stats,0,sizeof(stats));opens=reads=closes=position=0;
    fault=fail_alloc=generated=0;
}
static void *reader(void *unused) {
    (void)unused;
    for(unsigned i=0;i<1000;++i) {
        assert(!asset_index_missing("vid/020.vid"));
        assert(asset_index_missing("vid/does_not_exist.vid"));
    }
    return NULL;
}
int main(void) {
    reset();errno=EDOM;
    assert(!asset_index_missing("vid/020.vid") && errno==EDOM);
    assert(opens==1 && closes==1 && reads==5);
    assert(!asset_index_missing("vid/game_logo.png")); /* case fold cannot hide an existing entry */
    assert(!asset_index_missing("vid/.hidden"));
    unsigned previous=reads;
    for(unsigned i=0;i<10000;++i) assert(asset_index_missing("vid/missing.png") && errno==ENOENT);
    assert(reads==previous && opens==1);
    const char *unsupported[]={"vid/../020.vid","vid/020.vid.","vid/020.vid ","vid/ab*.png","vid/a:b.png","vid/sub/020.vid","vid\\020.vid","elsewhere/020.vid","vid/",NULL};
    for(unsigned i=0;unsupported[i];++i) assert(!asset_index_missing(unsupported[i]));
    assert(!asset_index_missing(NULL));assert(opens==1);
    assert(asset_index_missing("menus/img/missing.png"));
    assert(asset_index_missing("menus/items/missing.png"));assert(opens==3 && closes==3);
    for(int f=1;f<=4;++f) {
        reset();fault=f;
        assert(!asset_index_missing("vid/missing.png")); /* unknown never becomes absence */
        assert(!asset_index_missing("vid/020.vid"));assert(opens==1);
        assert(directories[0].state==-1 && !directories[0].hashes);
    }
    reset();fail_alloc=1;
    assert(!asset_index_missing("vid/missing.png"));assert(closes==1 && !directories[0].hashes);
    reset();generated=INDEX_MAX_ENTRIES+1;
    assert(!asset_index_missing("vid/missing.png"));assert(!directories[0].hashes && closes==1);
    reset();generated=INDEX_MAX_ENTRIES;
    assert(asset_index_missing("vid/missing.png"));assert(directories[0].capacity==INDEX_MAX_ENTRIES);
    assert(!asset_index_missing("vid/image_8191.png"));
    /* Deliberate hash collision must return the original path, not absence. */
    uint64_t collision;assert(name_hash("collision.png",&collision));
    directories[0].count=1;directories[0].hashes[0]=collision;
    assert(!asset_index_missing("vid/collision.png"));
    reset();pthread_t workers[4];
    for(unsigned i=0;i<4;++i) assert(!pthread_create(&workers[i],NULL,reader,NULL));
    for(unsigned i=0;i<4;++i) assert(!pthread_join(workers[i],NULL));
    assert(opens==1 && stats.present==4000 && stats.absent==4000);
    asset_index_report();assert(stats.lookups==0 && directories[0].state==1);
    reset();puts("Asset index regression passed: complete listing, present/case aliases, negative reuse, collisions, conservative paths, read/close/OOM/cap failures, concurrent readers");
}
