/* MIT. Shared immutable bytes; AAsset retains its own cursor. */
#ifndef ZOMBIE_ASSET_CACHE_H
#define ZOMBIE_ASSET_CACHE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct SharedAssetData SharedAssetData;
typedef struct { unsigned hits,misses,evictions,avoided_opens,fallback,entries,pinned;
    uint64_t saved_read_bytes; size_t bytes,peak_bytes,limit; } AssetCacheStats;
void asset_cache_configure(size_t limit);
SharedAssetData *asset_cache_acquire(const char *key);
/* On success takes ownership of *bytes, possibly replacing it with an existing
 * backing. On failure caller retains ownership. Never performs filesystem I/O. */
SharedAssetData *asset_cache_adopt(const char *key,unsigned char **bytes,size_t size);
void asset_cache_release(SharedAssetData *data);
const unsigned char *asset_cache_data(const SharedAssetData *data);
size_t asset_cache_size(const SharedAssetData *data);
void asset_cache_snapshot(AssetCacheStats *out);
typedef struct { char key[256];unsigned opens,hits,closes;uint64_t read_bytes;size_t size; } AssetPathStats;
void asset_cache_note_close(const char *key,size_t read_bytes,size_t size);
unsigned asset_cache_paths(AssetPathStats out[5],unsigned *overflow);
#ifdef __cplusplus
}
#endif
#endif
