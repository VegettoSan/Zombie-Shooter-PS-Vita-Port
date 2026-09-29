/* Zombie Shooter fast read-only AAsset backend.
 *
 * FalsoNDK's generic implementation uses FILE plus seek-to-end/ftell/seek-back
 * for every successful open.  Real-Vita profiling of build 56 showed startup
 * performing thousands of AAsset opens and spending ~78 seconds inside
 * successful open calls while actual reads took well under a second.  Assets
 * in this port are ordinary unpacked files, so use sceIo directly and query the
 * size once with sceIoGetstat.  Small/VID assets still feed the existing bounded
 * LRU; the guest ABI remains the normal opaque AAsset pointer.
 */
#include "utils/asset_fast.h"
#include "utils/asset_cache.h"

#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ZombieAsset {
    char *path;
    SharedAssetData *shared;
    unsigned char *buffer;
    SceUID fd;
    size_t position;
    size_t bytes_read;
    size_t size;
};

static char *asset_path(const char *filename) {
    if (!filename) return NULL;
    size_t base = strlen(DATA_PATH "assets/");
    size_t name = strlen(filename);
    if (!name || name > 480 || base + name + 1 > 768) return NULL;
    char *path = (char *)malloc(base + name + 1);
    if (!path) return NULL;
    memcpy(path, DATA_PATH "assets/", base);
    memcpy(path + base, filename, name + 1);
    for (char *p = path + base; *p; ++p) if (*p == '\\') *p = '/';
    return path;
}

static int should_buffer(const char *path, size_t size) {
    if (size <= 256u * 1024u) return 1;
    const char *ext = strrchr(path, '.');
    return ext && !strcmp(ext, ".vid") && size <= 2u * 1024u * 1024u;
}

static int read_all(SceUID fd, unsigned char *dst, size_t size) {
    size_t done = 0;
    while (done < size) {
        unsigned request = (size - done) > 1024u * 1024u ? 1024u * 1024u : (unsigned)(size - done);
        int n = sceIoRead(fd, dst + done, request);
        if (n <= 0) return 0;
        done += (size_t)n;
    }
    return 1;
}

extern "C" AAsset *zombie_asset_open(AAssetManager *mgr, const char *filename, int mode) {
    (void)mgr;
    (void)mode;
    char *path = asset_path(filename);
    if (!path) { errno = ENOENT; return NULL; }

    ZombieAsset *a = (ZombieAsset *)calloc(1, sizeof(*a));
    if (!a) { free(path); errno = ENOMEM; return NULL; }
    a->path = path;
    a->fd = -1;

    a->shared = asset_cache_acquire(path);
    if (a->shared) {
        a->buffer = const_cast<unsigned char *>(asset_cache_data(a->shared));
        a->size = asset_cache_size(a->shared);
        return (AAsset *)a;
    }

    SceIoStat st;
    memset(&st, 0, sizeof(st));
    if (sceIoGetstat(path, &st) < 0 || st.st_size < 0 || (uint64_t)st.st_size > (uint64_t)SIZE_MAX) {
        free(a->path); free(a); errno = ENOENT; return NULL;
    }
    a->size = (size_t)st.st_size;

    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) {
        free(a->path); free(a); errno = ENOENT; return NULL;
    }

    if (should_buffer(path, a->size)) {
        unsigned char *bytes = (unsigned char *)malloc(a->size ? a->size : 1);
        if (bytes && (a->size == 0 || read_all(fd, bytes, a->size))) {
            sceIoClose(fd);
            a->shared = asset_cache_adopt(path, &bytes, a->size);
            a->buffer = bytes;
            return (AAsset *)a;
        }
        free(bytes);
        /* A failed/allocation-limited buffer attempt must not fail the asset.
         * Rewind the same fd and retain normal streaming semantics. */
        if (sceIoLseek(fd, 0, SEEK_SET) < 0) {
            sceIoClose(fd); free(a->path); free(a); errno = EIO; return NULL;
        }
    }

    a->fd = fd;
    return (AAsset *)a;
}

extern "C" void zombie_asset_close(AAsset *asset) {
    if (!asset) return;
    ZombieAsset *a = (ZombieAsset *)asset;
    asset_cache_note_close(a->path, a->bytes_read, a->size);
    if (a->fd >= 0) sceIoClose(a->fd);
    if (a->shared) asset_cache_release(a->shared);
    else free(a->buffer);
    free(a->path);
    free(a);
}

extern "C" int zombie_asset_read(AAsset *asset, void *buf, size_t count) {
    if (!asset || (!buf && count)) { errno = EINVAL; return -1; }
    ZombieAsset *a = (ZombieAsset *)asset;
    if (a->position > a->size) return -1;
    size_t available = a->size - a->position;
    if (count > available) count = available;
    if (!count) return 0;

    int n;
    if (a->buffer) {
        memcpy(buf, a->buffer + a->position, count);
        n = (int)count;
    } else {
        if (a->fd < 0) return -1;
        size_t request = count > (size_t)INT32_MAX ? (size_t)INT32_MAX : count;
        n = sceIoRead(a->fd, buf, (unsigned)request);
        if (n < 0) { errno = EIO; return -1; }
    }
    a->position += (size_t)n;
    a->bytes_read += (size_t)n;
    return n;
}

extern "C" off_t zombie_asset_seek(AAsset *asset, off_t offset, int whence) {
    if (!asset) return (off_t)-1;
    ZombieAsset *a = (ZombieAsset *)asset;
    if (a->buffer) {
        int64_t base = whence == SEEK_SET ? 0 :
                       whence == SEEK_CUR ? (int64_t)a->position :
                       whence == SEEK_END ? (int64_t)a->size : -1;
        int64_t next = base + (int64_t)offset;
        if (base < 0 || next < 0 || (uint64_t)next > (uint64_t)a->size) return (off_t)-1;
        a->position = (size_t)next;
        return (off_t)next;
    }
    if (a->fd < 0) return (off_t)-1;
    SceOff next = sceIoLseek(a->fd, (SceOff)offset, whence);
    if (next < 0) { errno = EIO; return (off_t)-1; }
    a->position = (size_t)next;
    return (off_t)next;
}

extern "C" off_t zombie_asset_get_length(AAsset *asset) {
    if (!asset) return (off_t)-1;
    return (off_t)((ZombieAsset *)asset)->size;
}

extern "C" off_t zombie_asset_get_remaining_length(AAsset *asset) {
    if (!asset) return (off_t)-1;
    ZombieAsset *a = (ZombieAsset *)asset;
    if (a->position > a->size) return 0;
    return (off_t)(a->size - a->position);
}

extern "C" int zombie_asset_open_file_descriptor(AAsset *asset, off_t *out_start, off_t *out_length) {
    if (!asset) return -1;
    ZombieAsset *a = (ZombieAsset *)asset;
    if (out_start) *out_start = 0;
    if (out_length) *out_length = (off_t)a->size;

    /* Existing port behavior: the Vita OpenSL backend does not consume Android
     * FD music sources. Avoid leaking descriptors for the game's .m4a probes. */
    const char *ext = strrchr(a->path, '.');
    if (ext && !strcmp(ext, ".m4a")) return -1;
    return open(a->path, O_RDONLY);
}
