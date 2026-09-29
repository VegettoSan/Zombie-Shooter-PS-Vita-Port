#ifndef ZOMBIE_ASSET_FAST_H
#define ZOMBIE_ASSET_FAST_H

#include <stddef.h>
#include <sys/types.h>
#include <falso_ndk/FalsoNDK.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Read-only AAsset backend for Zombie Shooter.  The Android object is opaque to
 * the guest, so these functions can replace FalsoNDK's stdio implementation
 * without changing the game ABI. */
AAsset *zombie_asset_open(AAssetManager *mgr, const char *filename, int mode);
void zombie_asset_close(AAsset *asset);
int zombie_asset_read(AAsset *asset, void *buf, size_t count);
off_t zombie_asset_seek(AAsset *asset, off_t offset, int whence);
off_t zombie_asset_get_length(AAsset *asset);
off_t zombie_asset_get_remaining_length(AAsset *asset);
int zombie_asset_open_file_descriptor(AAsset *asset, off_t *out_start, off_t *out_length);

#ifdef __cplusplus
}
#endif

#endif
