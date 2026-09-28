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

#ifdef ZOMBIE_DEBUG_BUILD
#include <errno.h>
static void allocation_failure(const char *op,size_t size,void *previous,void *caller) {
    static unsigned failures;
    if(__atomic_fetch_add(&failures,1,__ATOMIC_RELAXED)>=8) return;
    int saved_errno=errno;
    struct mallinfo heap=mallinfo();
    l_error("[MEM] guest %s failed bytes=%u previous=%p caller=%p errno=%d heap_arena=%u allocated=%u free=%u",op,(unsigned)size,previous,caller,saved_errno,(unsigned)heap.arena,(unsigned)heap.uordblks,(unsigned)heap.fordblks);
    logger_force_sync();errno=saved_errno;
}
void *malloc_soloader_diagnostic(size_t size) {
    void *result=malloc(size);
    if(!result && size) allocation_failure("malloc",size,NULL,__builtin_return_address(0));
    return result;
}
void *calloc_soloader_diagnostic(size_t count,size_t size) {
    void *result=calloc(count,size);
    if(!result && count && size) allocation_failure("calloc",count<=SIZE_MAX/size?count*size:SIZE_MAX,NULL,__builtin_return_address(0));
    return result;
}
void *realloc_soloader_diagnostic(void *previous,size_t size) {
    void *result=realloc(previous,size);
    if(!result && size) allocation_failure("realloc",size,previous,__builtin_return_address(0));
    return result;
}
#endif
