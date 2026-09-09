/*
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/logger.h"

#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>

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

#define LOG_FILE_PATH DATA_PATH "log.txt"

static SceKernelLwMutexWork _log_mutex;
static atomic_bool _log_mutex_ready = ATOMIC_VAR_INIT(false);
static SceUID _log_fd = -1;
static atomic_bool _log_file_ready = ATOMIC_VAR_INIT(false);

// Buffer A is used to adjust the format string (with colors for console).
static char buffer_a[2048];
// Buffer B is used to compile the final log using the updated format string.
static char buffer_b[2048];
// Buffer C is plain text version for the file (no ANSI colors).
static char buffer_c[2048];

static void _log_open_file(void) {
    if (atomic_load_explicit(&_log_file_ready, memory_order_relaxed))
        return;

    // Create directory if needed (best effort)
    sceIoMkdir(DATA_PATH, 0777);

    _log_fd = sceIoOpen(LOG_FILE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (_log_fd >= 0) {
        const char *header = "=== Zombie Shooter Vita Port Log ===\n";
        sceIoWrite(_log_fd, header, strlen(header));
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
            sceIoWrite(_log_fd, line, (size_t)n);
            // Flush is not strictly available; next write or close will persist.
        }
    }

    if (atomic_load_explicit(&_log_mutex_ready, memory_order_relaxed)) {
        sceKernelUnlockLwMutex(&_log_mutex, 1);
    }
}
