/*
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 * Copyright (C) 2026      VegettoSan
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  _struct_converters.c
 * @brief Converters for `dirent` struct, `stat` struct, and `open()` flags
 *        that deal with newlib (Vita) and bionic (Android) incompatibilities.
 *
 * This file has to be `#include`d in `reimpl/io.c` and not compiled on its own.
 */

#define SC_INLINE static inline __attribute__((always_inline))

#define BIONIC_O_WRONLY                                          01
#define BIONIC_O_RDWR                                            02
#define BIONIC_O_CREAT                                         0100
#define BIONIC_O_EXCL                                          0200
#define BIONIC_O_TRUNC                                        01000
#define BIONIC_O_APPEND                                       02000
#define BIONIC_O_NONBLOCK                                     04000
#define BIONIC_O_DIRECTORY                                 00200000
#define BIONIC__O_TMPFILE                                 020000000
#define BIONIC_O_TMPFILE   (BIONIC__O_TMPFILE | BIONIC_O_DIRECTORY)

/**
 * Convert bionic (Android) `open()` flags to newlib (Vita) flags
 *
 * @param[in] flags open() flags created using bionic defines
 *
 * @return open(flags) recreated using newlib defines
 */
SC_INLINE int oflags_bionic_to_newlib(int flags) {
    int out = 0;
    if (flags & BIONIC_O_RDWR)
        out |= O_RDWR;
    else if (flags & BIONIC_O_WRONLY)
        out |= O_WRONLY;
    else
        out |= O_RDONLY;
    if (flags & BIONIC_O_NONBLOCK)
        out |= O_NONBLOCK;
    if (flags & BIONIC_O_APPEND)
        out |= O_APPEND;
    if ((flags & BIONIC_O_CREAT) || (flags & BIONIC_O_TMPFILE))
        out |= O_CREAT;
    if (flags & BIONIC_O_TRUNC)
        out |= O_TRUNC;
    if (flags & BIONIC_O_EXCL)
        out |= O_EXCL;
    return out;
}

/**
 * Convert newlib (Vita) `dirent` struct to bionic (Android) format.
 *
 * @param[in] dirent_newlib Pointer to a newlib-format dirent struct
 *
 * @return Pointer to a bionic-format dirent struct.
 *         Must be freed by the caller.
 */
SC_INLINE
dirent64_bionic * dirent_newlib_to_bionic(const struct dirent* dirent_newlib) {
    dirent64_bionic * ret = malloc(sizeof(dirent64_bionic));
    if (!ret)
        return NULL;
    memset(ret, 0, sizeof(*ret));
    strncpy(ret->d_name, dirent_newlib->d_name, sizeof(ret->d_name) - 1);
    ret->d_off = 0;
    ret->d_reclen = 0;
    ret->d_type = SCE_S_ISDIR(dirent_newlib->d_stat.st_mode) ? DT_DIR : DT_REG;
    return ret;
}

/**
 * Convert newlib (Vita) `stat` struct to the exact Android/Bionic ARM32
 * stat64 byte layout expected by the guest .so.
 *
 * Do not assign Vita libc structs/typedefs wholesale here: nlink_t/uid_t/gid_t
 * differ in width between Vita/newlib and Android/Bionic. The destination is
 * intentionally fixed-width and is zeroed first so every ABI padding byte is
 * deterministic.
 *
 * @param[in]  src Pointer to a newlib-format stat struct
 * @param[out] dst Pointer to a bionic-format stat struct
 */
SC_INLINE
void stat_newlib_to_bionic(const struct stat * src, stat64_bionic * dst) {
    memset(dst, 0, sizeof(*dst));

    dst->st_dev = (uint64_t) src->st_dev;
    dst->__st_ino = (uint32_t) src->st_ino;
    dst->st_ino = (uint64_t) src->st_ino;
    dst->st_mode = (uint32_t) src->st_mode;
    dst->st_nlink = (uint32_t) src->st_nlink;
    dst->st_uid = (uint32_t) src->st_uid;
    dst->st_gid = (uint32_t) src->st_gid;
    dst->st_rdev = (uint64_t) src->st_rdev;
    dst->st_size = (int64_t) src->st_size;
    dst->st_blksize = (uint32_t) src->st_blksize;
    dst->st_blocks = (uint64_t) src->st_blocks;
    dst->st_atim_sec = (int32_t) src->st_atime;
    dst->st_atim_nsec = 0;
    dst->st_mtim_sec = (int32_t) src->st_mtime;
    dst->st_mtim_nsec = 0;
    dst->st_ctim_sec = (int32_t) src->st_ctime;
    dst->st_ctim_nsec = 0;
}
