/* Exact absence index for immutable packaged assets (MIT, port license).
 * Store sorted hashes: collisions cause an extra original open, never a false
 * absence. Positive opens/bytes/ownership/stdio buffering remain unchanged. */
#include "utils/asset_index.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#ifndef ZOMBIE_ASSET_INDEX_HOST
#include <psp2/io/dirent.h>
#include <psp2/kernel/processmgr.h>
#include "utils/logger.h"
#endif
#ifndef ASSET_INDEX_REALLOC
#define ASSET_INDEX_REALLOC realloc
#endif
#define INDEX_MAX_ENTRIES 8192u
static struct DirectoryIndex {
    const char *prefix;
    uint64_t *hashes;
    unsigned count, capacity;
    int state; /* 0 unbuilt, 1 complete, -1 uncertain: original path only */
} directories[]={{"vid/",0,0,0,0},{"menus/img/",0,0,0,0},{"menus/items/",0,0,0,0}};
static pthread_mutex_t index_lock=PTHREAD_MUTEX_INITIALIZER;
static struct { unsigned lookups,present,absent,fallback,builds,failed,listed,build_us; } stats;
/* Conservative ASCII leaves. Avoid aliases, separators, wildcard/path syntax,
 * trailing dots and Unicode filesystem case folding. Unknown names fall back. */
static int name_hash(const char *name,uint64_t *hash) {
    uint64_t h=UINT64_C(14695981039346656037);
    unsigned n=0;unsigned char last=0;
    for(;n<256 && name[n];++n) {
        unsigned char c=(unsigned char)name[n];
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.')) return 0;
        last=c;if(c>='A'&&c<='Z') c+=32;
        h=(h^c)*UINT64_C(1099511628211);
    }
    if(!n || n==256 || last=='.') return 0;
    *hash=h;return 1;
}
static int compare_hash(const void *a,const void *b) {
    uint64_t x=*(const uint64_t *)a,y=*(const uint64_t *)b;
    return (x>y)-(x<y);
}
static void build_index(struct DirectoryIndex *d) {
    uint64_t start=sceKernelGetProcessTimeWide();
    stats.builds++;d->state=-1;
    char path[256];
    const char base[]=DATA_PATH "assets/";
    size_t size=sizeof(base)-1+strlen(d->prefix)+1;
    if(size>sizeof(path)) { stats.failed++;return; }
    memcpy(path,base,sizeof(base)-1);strcpy(path+sizeof(base)-1,d->prefix);
    int fd=sceIoDopen(path);
    int complete=fd>=0;
    if(fd>=0) {
        SceIoDirent entry;int result;
        for(;;) {
            memset(&entry,0,sizeof(entry));result=sceIoDread(fd,&entry);
            if(result<=0) { if(result<0) complete=0;break; }
            if(!strcmp(entry.d_name,".") || !strcmp(entry.d_name,"..")) continue;
            uint64_t hash;
            if(!name_hash(entry.d_name,&hash) || d->count==INDEX_MAX_ENTRIES) { complete=0;break; }
            if(d->count==d->capacity) {
                unsigned capacity=d->capacity?d->capacity*2:64;
                uint64_t *next=ASSET_INDEX_REALLOC(d->hashes,capacity*sizeof(uint64_t));
                if(!next) { complete=0;break; }
                d->hashes=next;d->capacity=capacity;
            }
            d->hashes[d->count++]=hash;
        }
        if(sceIoDclose(fd)<0) complete=0;
    }
    if(complete) {
        if(d->count>1) qsort(d->hashes,d->count,sizeof(uint64_t),compare_hash);
        d->state=1;stats.listed+=d->count;
    } else {
        free(d->hashes);d->hashes=NULL;d->count=d->capacity=0;stats.failed++;
    }
    stats.build_us+=(unsigned)(sceKernelGetProcessTimeWide()-start);
}
int asset_index_missing(const char *filename) {
    int saved_errno=errno;
    struct DirectoryIndex *d=NULL;uint64_t hash;
    if(filename) for(unsigned i=0;i<sizeof(directories)/sizeof(directories[0]);++i) {
        size_t n=strlen(directories[i].prefix);
        if(!strncmp(filename,directories[i].prefix,n) && name_hash(filename+n,&hash)) { d=&directories[i];break; }
    }
    pthread_mutex_lock(&index_lock);
    int missing=0;
    if(!d) stats.fallback++;
    else {
        stats.lookups++;
        if(!d->state) build_index(d);
        if(d->state!=1) stats.fallback++;
        else {
            unsigned lo=0,hi=d->count;
            while(lo<hi) { unsigned mid=lo+(hi-lo)/2;if(d->hashes[mid]<hash) lo=mid+1;else hi=mid; }
            missing=lo==d->count || d->hashes[lo]!=hash;
            if(missing) stats.absent++;else stats.present++;
        }
    }
    pthread_mutex_unlock(&index_lock);
    errno=missing?ENOENT:saved_errno;
    return missing;
}
void asset_index_report(void) {
    pthread_mutex_lock(&index_lock);
    unsigned bytes=0,entries=0;
    for(unsigned i=0;i<sizeof(directories)/sizeof(directories[0]);++i) {
        bytes+=directories[i].capacity*sizeof(uint64_t);entries+=directories[i].count;
    }
    unsigned lookups=stats.lookups,present=stats.present,absent=stats.absent,fallback=stats.fallback;
    unsigned builds=stats.builds,failed=stats.failed,listed=stats.listed,us=stats.build_us;
    memset(&stats,0,sizeof(stats));pthread_mutex_unlock(&index_lock);
    l_perf("asset_index lookups=%u present=%u absent=%u fallback=%u builds=%u build_failures=%u listed=%u build_us=%u entries=%u bytes=%u",lookups,present,absent,fallback,builds,failed,listed,us,entries,bytes);
}
