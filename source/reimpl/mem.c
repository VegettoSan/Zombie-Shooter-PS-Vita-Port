/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/mem.h"
#include "utils/logger.h"
#include "utils/perf.h"
#include <psp2/kernel/processmgr.h>

#include <string.h>
#include <malloc.h>
#include <psp2/kernel/clib.h>

/* Same SDK memset (already a sceClibMemset thunk). Only large guest fills
 * are timed, including the game's ~5 MiB software framebuffer clear. */
void *memset_soloader_perf(void *dst,int c,size_t len) {
    if(len<256*1024) return memset(dst,c,len);
    uint64_t start=sceKernelGetProcessTimeWide();
    void *ret=memset(dst,c,len);
    perf_bulk_memset(len,start);
    return ret;
}

void *sceClibMemclr(void *dst, size_t len) {
    return sceClibMemset(dst, 0, len);
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offs) {
    l_warn("mmap(%p, %i, %i, %i, %i, %li)", addr, length, prot, flags, fd, offs);

    if (length <= 0) {
        return MAP_FAILED;
    }
    void* ret= malloc(length);
    memset(ret, 0, length);
    return ret;
}

int munmap(void *addr, size_t length) {
    if (addr) free(addr);
    return 0;
}
