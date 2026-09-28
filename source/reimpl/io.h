/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022-2024 Rinnegatamante / Volodymyr Atamanenko
 * Copyright (C) 2026      VegettoSan
 *
 * This software may be modified and distributed under the terms of the MIT
 * license. See the LICENSE file for details.
 */

/**
 * @file  io.h
 * @brief Wrappers and implementations for some of the IO functions.
 */

#ifndef SOLOADER_IO_H
#define SOLOADER_IO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/dirent.h>
#include <sys/syslimits.h>
#include <sys/fcntl.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifndef DT_DIR
#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define DT_WHT 14
#endif

/*
 * Android/Bionic ARM32 stat/stat64 ABI.
 *
 * IMPORTANT: never use Vita/newlib's nlink_t/uid_t/gid_t/time_t directly in
 * this guest-facing struct. On Vita/newlib several of those typedefs are
 * 16-bit, while Android 7/Bionic ARM32 uses 32-bit nlink/uid/gid and a fixed
 * layout. A size/offset mismatch shifts st_size and timestamps, so a game can
 * successfully write a save and then reject it as zero-sized/corrupt when it
 * calls stat()/fstat().
 *
 * The explicit padding below reproduces the natural ARM EABI layout of
 * Bionic's struct stat64 while keeping this host compiler independent from
 * its own libc typedef widths. Android 7 ARM32 expected size: 104 bytes.
 *
 * Reference: AOSP bionic libc/include/sys/stat.h + sys/types.h (ARM32), and
 * the same class of save bug documented/fixed in MetalSyntax/ILLUSIA-2-Vita.
 */
typedef struct __attribute__((__packed__)) stat64_bionic {
    uint64_t st_dev;          /* 0x00 */
    uint8_t  __pad0[4];       /* 0x08 */
    uint32_t __st_ino;        /* 0x0C */
    uint32_t st_mode;         /* 0x10 */
    uint32_t st_nlink;        /* 0x14 */
    uint32_t st_uid;          /* 0x18 */
    uint32_t st_gid;          /* 0x1C */
    uint64_t st_rdev;         /* 0x20 */
    uint8_t  __pad3[4];       /* 0x28: Bionic field */
    uint8_t  __pad4[4];       /* 0x2C: ARM EABI alignment before int64 */
    int64_t  st_size;         /* 0x30 */
    uint32_t st_blksize;      /* 0x38 */
    uint8_t  __pad5[4];       /* 0x3C: ARM EABI alignment before uint64 */
    uint64_t st_blocks;       /* 0x40 */
    int32_t  st_atim_sec;     /* 0x48 */
    int32_t  st_atim_nsec;    /* 0x4C */
    int32_t  st_mtim_sec;     /* 0x50 */
    int32_t  st_mtim_nsec;    /* 0x54 */
    int32_t  st_ctim_sec;     /* 0x58 */
    int32_t  st_ctim_nsec;    /* 0x5C */
    uint64_t st_ino;          /* 0x60 */
} stat64_bionic;

/*
 * Android/Bionic dirent/dirent64 ABI on 32-bit Android.
 * Both public structures are identical: uint64 inode, int64 offset,
 * uint16 record length, uint8 type, char name[256], then natural tail
 * padding to an 8-byte-aligned total of 280 bytes. The old Vita wrapper used
 * a 16-bit inode and 64-bit record length, shifting every field after d_ino.
 */
typedef struct __attribute__((__packed__)) dirent64_bionic {
    uint64_t d_ino;           /* 0x00 */
    int64_t  d_off;           /* 0x08 */
    uint16_t d_reclen;        /* 0x10 */
    uint8_t  d_type;          /* 0x12 */
    char     d_name[256];     /* 0x13 */
    uint8_t  __pad_tail[5];   /* 0x113 -> sizeof 280 */
} dirent64_bionic;

#if defined(__cplusplus)
#define ZS_STATIC_ASSERT(cond, msg) static_assert((cond), msg)
#else
#define ZS_STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#endif

ZS_STATIC_ASSERT(sizeof(stat64_bionic) == 104, "ARM32 Bionic stat64 must be 104 bytes");
ZS_STATIC_ASSERT(offsetof(stat64_bionic, st_nlink) == 0x14, "Bionic st_nlink offset mismatch");
ZS_STATIC_ASSERT(offsetof(stat64_bionic, st_size) == 0x30, "Bionic st_size offset mismatch");
ZS_STATIC_ASSERT(offsetof(stat64_bionic, st_blocks) == 0x40, "Bionic st_blocks offset mismatch");
ZS_STATIC_ASSERT(offsetof(stat64_bionic, st_atim_sec) == 0x48, "Bionic st_atim offset mismatch");
ZS_STATIC_ASSERT(offsetof(stat64_bionic, st_ino) == 0x60, "Bionic st_ino offset mismatch");

ZS_STATIC_ASSERT(sizeof(dirent64_bionic) == 280, "ARM32 Bionic dirent64 must be 280 bytes");
ZS_STATIC_ASSERT(offsetof(dirent64_bionic, d_off) == 0x08, "Bionic dirent d_off offset mismatch");
ZS_STATIC_ASSERT(offsetof(dirent64_bionic, d_reclen) == 0x10, "Bionic dirent d_reclen offset mismatch");
ZS_STATIC_ASSERT(offsetof(dirent64_bionic, d_type) == 0x12, "Bionic dirent d_type offset mismatch");
ZS_STATIC_ASSERT(offsetof(dirent64_bionic, d_name) == 0x13, "Bionic dirent d_name offset mismatch");

#undef ZS_STATIC_ASSERT

int open_soloader(const char * path, int oflag, ...);

FILE * fopen_soloader(const char * filename, const char * mode);

DIR *opendir_soloader(char *name);

int stat_soloader(const char * path, stat64_bionic * buf);

int fstat_soloader(int fd, stat64_bionic * buf);

struct dirent64_bionic * readdir_soloader(DIR *dir);

int readdir_r_soloader(DIR * dirp, dirent64_bionic * entry,
                       dirent64_bionic ** result);

int close_soloader(int fd);

int fclose_soloader(FILE *f);

int closedir_soloader(DIR *dir);

int fcntl_soloader(int fd, int cmd, ...);

int ioctl_soloader(int fd, int request, ... /* arg */);

int fsync_soloader(int fd);
size_t fwrite_soloader(const void *,size_t,size_t,FILE *);
ssize_t write_soloader(int,const void *,size_t);
int rename_soloader(const char *,const char *);
int unlink_soloader(const char *);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_IO_H
