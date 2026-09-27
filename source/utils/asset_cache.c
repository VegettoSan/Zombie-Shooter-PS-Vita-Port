/* MIT. Bounded LRU including pinned bytes. No eviction of live handles.
 * Admission failure preserves the existing per-handle buffering/FILE path. */
#include "utils/asset_cache.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#define CACHE_SLOTS 512
#define CACHE_KEY_MAX 256
struct SharedAssetData { char key[CACHE_KEY_MAX]; unsigned char *bytes;
    size_t size; unsigned refs; uint64_t use; };
static SharedAssetData entries[CACHE_SLOTS];
static pthread_mutex_t cache_lock=PTHREAD_MUTEX_INITIALIZER;
static AssetCacheStats stats;
static uint64_t sequence;
static AssetPathStats paths[128];static unsigned path_count,path_overflow;
static AssetPathStats *path_record(const char *key) {
    if(!key || strlen(key)>=256) { path_overflow++;return NULL; }
    for(unsigned i=0;i<path_count;i++) if(!strcmp(paths[i].key,key)) return paths+i;
    if(path_count==128) { path_overflow++;return NULL; }
    AssetPathStats *p=paths+path_count++;strcpy(p->key,key);return p;
}
void asset_cache_note_close(const char *key,size_t bytes,size_t size) {
    pthread_mutex_lock(&cache_lock);AssetPathStats *p=path_record(key);
    if(p) { p->closes++;p->read_bytes+=bytes;p->size=size; }pthread_mutex_unlock(&cache_lock);
}
unsigned asset_cache_paths(AssetPathStats out[5],unsigned *overflow) {
    pthread_mutex_lock(&cache_lock);unsigned n=path_count<5?path_count:5;
    for(unsigned i=0;i<n;i++) { unsigned best=i;
        for(unsigned j=i+1;j<path_count;j++) if(paths[j].opens>paths[best].opens) best=j;
        AssetPathStats tmp=paths[i];paths[i]=paths[best];paths[best]=tmp;out[i]=paths[i]; }
    *overflow=path_overflow;memset(paths,0,sizeof(paths));path_count=path_overflow=0;
    pthread_mutex_unlock(&cache_lock);return n;
}
static SharedAssetData *find(const char *key) {
    for(unsigned i=0;i<CACHE_SLOTS;i++) if(entries[i].bytes && !strcmp(entries[i].key,key)) return entries+i;
    return NULL;
}
static void evict(SharedAssetData *e) {
    stats.bytes-=e->size;stats.entries--;stats.evictions++;
    free(e->bytes);memset(e,0,sizeof(*e));
}
static SharedAssetData *oldest(void) {
    SharedAssetData *best=NULL;
    for(unsigned i=0;i<CACHE_SLOTS;i++) if(entries[i].bytes && !entries[i].refs && (!best || entries[i].use<best->use)) best=entries+i;
    return best;
}
void asset_cache_configure(size_t limit) {
    pthread_mutex_lock(&cache_lock);stats.limit=limit;
    while(stats.bytes>limit) { SharedAssetData *e=oldest();if(!e) break;evict(e); }
    pthread_mutex_unlock(&cache_lock);
}
SharedAssetData *asset_cache_acquire(const char *key) {
    pthread_mutex_lock(&cache_lock);
    SharedAssetData *e=key && stats.limit?find(key):NULL;
    AssetPathStats *p=path_record(key);if(p) { p->opens++;if(e) { p->hits++;p->size=e->size; } }
    if(e) { if(!e->refs) stats.pinned++;e->refs++;e->use=++sequence;
        stats.hits++;stats.avoided_opens++;stats.saved_read_bytes+=e->size; }
    else stats.misses++;
    pthread_mutex_unlock(&cache_lock);return e;
}
SharedAssetData *asset_cache_adopt(const char *key,unsigned char **bytes,size_t size) {
    if(!key || !bytes || !*bytes || strlen(key)>=CACHE_KEY_MAX) return NULL;
    pthread_mutex_lock(&cache_lock);
    SharedAssetData *e=find(key);
    if(e) { free(*bytes);*bytes=e->bytes;if(!e->refs) stats.pinned++;e->refs++;e->use=++sequence; }
    else if(size<=stats.limit && stats.limit) {
        while(stats.bytes>stats.limit-size) { SharedAssetData *victim=oldest();if(!victim) break;evict(victim); }
        if(stats.bytes<=stats.limit-size) {
            for(unsigned i=0;i<CACHE_SLOTS;i++) if(!entries[i].bytes) { e=entries+i;break; }
            if(!e) { e=oldest();if(e) evict(e); }
            if(e) { strcpy(e->key,key);e->bytes=*bytes;e->size=size;e->refs=1;e->use=++sequence;
                stats.bytes+=size;stats.entries++;stats.pinned++;
                if(stats.bytes>stats.peak_bytes) stats.peak_bytes=stats.bytes; }
        }
    }
    if(!e) stats.fallback++;
    pthread_mutex_unlock(&cache_lock);return e;
}
void asset_cache_release(SharedAssetData *e) {
    if(!e) return;
    pthread_mutex_lock(&cache_lock);
    if(e->refs && !--e->refs) { stats.pinned--;e->use=++sequence;if(stats.bytes>stats.limit) evict(e); }
    pthread_mutex_unlock(&cache_lock);
}
const unsigned char *asset_cache_data(const SharedAssetData *e) { return e?e->bytes:NULL; }
size_t asset_cache_size(const SharedAssetData *e) { return e?e->size:0; }
void asset_cache_snapshot(AssetCacheStats *out) {
    pthread_mutex_lock(&cache_lock);*out=stats;pthread_mutex_unlock(&cache_lock);
}
