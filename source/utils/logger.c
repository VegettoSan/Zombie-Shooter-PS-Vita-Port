/*
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/logger.h"

#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>

#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>

#define COLOR_RED    "\x1B[38;5;196m"
#define COLOR_PINK   "\x1B[38;5;212m"
#define COLOR_ORANGE "\x1B[38;5;202m"
#define COLOR_BLUE   "\x1B[38;5;32m"
#define COLOR_GREEN  "\x1B[32m"
#define COLOR_CYAN   "\x1B[36m"

#define COLOR_END    "\033[0m"

#ifndef DATA_PATH
#define DATA_PATH "ux0:data/zombieshooter/"
#endif

#define LOG_DIR_PATH DATA_PATH "logs"

static SceKernelLwMutexWork _log_mutex;
static atomic_bool _log_mutex_ready = ATOMIC_VAR_INIT(false);
static SceUID _log_fd = -1;
static atomic_bool _log_file_ready = ATOMIC_VAR_INIT(false);
static char _log_path[128];
static unsigned _log_unsynced_lines;
static volatile unsigned _log_sync_count;
static volatile unsigned _log_sync_us;

void logger_get_sync_stats(unsigned *count, unsigned *microseconds) {
    if (count) *count = __atomic_load_n(&_log_sync_count, __ATOMIC_RELAXED);
    if (microseconds) *microseconds = __atomic_load_n(&_log_sync_us, __ATOMIC_RELAXED);
}

// Buffer A is used to adjust the format string (with colors for console).
static char buffer_a[2048];
// Buffer B is used to compile the final log using the updated format string.
static char buffer_b[2048];
// Buffer C is plain text version for the file (no ANSI colors).
static char buffer_c[2048];

static void _log_open_file(void) {
    if (atomic_load_explicit(&_log_file_ready, memory_order_relaxed))
        return;

    // Preserve each hardware run; a crash must not erase the previous trace.
    sceIoMkdir(DATA_PATH, 0777);
    sceIoMkdir(LOG_DIR_PATH, 0777);
    for (unsigned run = 1; run <= 9999; ++run) {
        SceIoStat stat;
        sceClibSnprintf(_log_path, sizeof(_log_path),
                        LOG_DIR_PATH "/log_%04u.log", run);
        if (sceIoGetstat(_log_path, &stat) >= 0)
            continue;
        _log_fd = sceIoOpen(_log_path,
                           SCE_O_WRONLY | SCE_O_CREAT | SCE_O_EXCL, 0666);
        break;
    }
    if (_log_fd < 0) {
        sceClibSnprintf(_log_path, sizeof(_log_path),
                        DATA_PATH "log_fallback.txt");
        _log_fd = sceIoOpen(_log_path,
                           SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0666);
    }
    if (_log_fd >= 0) {
        char header[192];
        int length = sceClibSnprintf(header, sizeof(header),
                                    "=== Zombie Shooter Vita Port: %s ===\n",
                                    _log_path);
        if (length > 0)
            sceIoWrite(_log_fd, header, (size_t)length);
        sceIoSyncByFd(_log_fd, 0);
        atomic_store_explicit(&_log_file_ready, true, memory_order_relaxed);
    }
}

void _log_print(int t, const char* fmt, ...) {
    if (!atomic_load_explicit(&_log_mutex_ready, memory_order_relaxed)) {
        int ret = sceKernelCreateLwMutex(&_log_mutex, "log_lock", 0, 0, NULL);
        if (ret < 0) {
            sceClibPrintf("Error: failed to create log mutex: 0x%x\n", ret);
            return;
        }
        atomic_store_explicit(&_log_mutex_ready, true, memory_order_relaxed);
    }
    sceKernelLockLwMutex(&_log_mutex, 1, NULL);

    _log_open_file();

    const char *tag = "info";
    switch (t) {
        case LT_DEBUG:
            tag = "debug";
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s• debug%s    %s\n",
                            COLOR_PINK, COLOR_END, fmt); break;
        case LT_INFO:
            tag = "info";
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %sℹ info%s     %s\n",
                            COLOR_BLUE, COLOR_END, fmt); break;
        case LT_WARN:
            tag = "warning";
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s⚠ warning%s  %s\n",
                            COLOR_ORANGE, COLOR_END, fmt); break;
        case LT_ERROR:
            tag = "error";
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s⨯ error%s    %s\n",
                            COLOR_RED, COLOR_END, fmt); break;
        case LT_FATAL:
            tag = "fatal";
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s! fatal%s    %s\n",
                            COLOR_RED, COLOR_END, fmt); break;
        case LT_SUCCESS:
            tag = "success";
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s! success%s  %s\n",
                            COLOR_GREEN, COLOR_END, fmt); break;
        case LT_WAIT:
            tag = "waiting";
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s… waiting%s  %s\n",
                            COLOR_CYAN, COLOR_END, fmt); break;
        default:
            if (atomic_load_explicit(&_log_mutex_ready, memory_order_relaxed)) {
                sceKernelUnlockLwMutex(&_log_mutex, 1);
            }
            return;
    }

    va_list list;
    va_start(list, fmt);
    sceClibVsnprintf(buffer_b, sizeof(buffer_b), buffer_a, list);
    va_end(list);

    // Console output (with colors)
    sceClibPrintf(buffer_b);

    // File output (plain text, no colors)
    if (atomic_load_explicit(&_log_file_ready, memory_order_relaxed) && _log_fd >= 0) {
        va_list list2;
        va_start(list2, fmt);
        sceClibVsnprintf(buffer_c, sizeof(buffer_c), fmt, list2);
        va_end(list2);

        char line[2200];
        int n = sceClibSnprintf(line, sizeof(line), "[%s] %s\n", tag, buffer_c);
        if (n > 0) {
            size_t write_len = (size_t)n;
            if (write_len >= sizeof(line))
                write_len = sizeof(line) - 1;
            sceIoWrite(_log_fd, line, write_len);
            /* Per-line sync dominated the 14k-line loading trace on Vita.
             * Keep warnings/errors crash-safe and checkpoint normal traces. */
            if (t == LT_WARN || t == LT_ERROR || t == LT_FATAL ||
                ++_log_unsynced_lines >= 32) {
                uint64_t start_us = sceKernelGetProcessTimeWide();
                sceIoSyncByFd(_log_fd, 0);
                unsigned spent_us = (unsigned)(sceKernelGetProcessTimeWide() - start_us);
                __atomic_add_fetch(&_log_sync_count, 1, __ATOMIC_RELAXED);
                __atomic_add_fetch(&_log_sync_us, spent_us, __ATOMIC_RELAXED);
                _log_unsynced_lines = 0;
            }
        }
    }

    if (atomic_load_explicit(&_log_mutex_ready, memory_order_relaxed)) {
        sceKernelUnlockLwMutex(&_log_mutex, 1);
    }
}
