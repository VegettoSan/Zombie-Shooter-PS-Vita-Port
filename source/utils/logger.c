/* Port logger: serialized formatting, buffered Release output and bounded sync. */
#include "utils/logger.h"
#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#ifndef DATA_PATH
#define DATA_PATH "ux0:data/zombieshooter/"
#endif
#ifndef ZOMBIE_BUILD_ID
#define ZOMBIE_BUILD_ID "unknown"
#define ZOMBIE_BUILD_VARIANT "unknown"
#endif
static SceKernelLwMutexWork mutex;
static unsigned init_state;
static SceUID fd = -1;
static unsigned unsynced, buffered;
static uint64_t last_sync;
static char file_buffer[8192];
static LoggerStats stats;
#ifdef ZOMBIE_RELEASE_BUILD
/* Full string equality: hash collisions never suppress a unique message. */
static struct { char text[2048]; unsigned count; } repeats[16];
static unsigned replacement;
#endif
static int lock_log(void) {
    unsigned expected = 0;
    if (__atomic_compare_exchange_n(&init_state, &expected, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        int result = sceKernelCreateLwMutex(&mutex, "log_lock", 0, 0, NULL);
        __atomic_store_n(&init_state, result < 0 ? 3 : 2, __ATOMIC_RELEASE);
    }
    while (__atomic_load_n(&init_state, __ATOMIC_ACQUIRE) == 1)
        sceKernelDelayThread(100);
    if (__atomic_load_n(&init_state, __ATOMIC_ACQUIRE) != 2) return 0;
    return sceKernelLockLwMutex(&mutex, 1, NULL) >= 0;
}
static void flush_locked(void) {
    unsigned done = 0;
    while (done < buffered) {
        int n = sceIoWrite(fd, file_buffer + done, buffered - done);
        if (n <= 0) break;
        done += (unsigned)n;
    }
    if (done) { buffered -= done; memmove(file_buffer, file_buffer + done, buffered); }
}
static void sync_locked(void) {
    if (fd < 0) return;
    uint64_t start = sceKernelGetProcessTimeWide();
    flush_locked();
    sceIoSyncByFd(fd, 0);
    stats.syncs++;
    stats.sync_us += (unsigned)(sceKernelGetProcessTimeWide() - start);
    last_sync = sceKernelGetProcessTimeWide();
    unsynced = 0;
}
void logger_get_stats(LoggerStats *out) {
    if (!out || !lock_log()) return;
    *out = stats;
    sceKernelUnlockLwMutex(&mutex, 1);
}
void logger_get_sync_stats(unsigned *count, unsigned *us) {
    LoggerStats s = {0}; logger_get_stats(&s);
    if (count) *count = s.syncs;
    if (us) *us = s.sync_us;
}
void logger_force_sync(void) {
    if (!lock_log()) return;
    sync_locked();
    sceKernelUnlockLwMutex(&mutex, 1);
}
static void open_locked(void) {
    if (fd >= 0) return;
    sceIoMkdir(DATA_PATH, 0777);
    sceIoMkdir(DATA_PATH "logs", 0777);
    char path[128];
    for (unsigned run = 1; run <= 9999; ++run) {
        SceIoStat st;
        sceClibSnprintf(path, sizeof(path), DATA_PATH "logs/log_%04u.log", run);
        if (sceIoGetstat(path, &st) >= 0) continue;
        fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_EXCL, 0666);
        break;
    }
    if (fd < 0) fd = sceIoOpen(DATA_PATH "log_fallback.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0666);
    if (fd >= 0) {
        const char header[] = "=== Zombie Shooter Vita Port ===\n=== BUILD variant=" ZOMBIE_BUILD_VARIANT " id=" ZOMBIE_BUILD_ID " ===\n";
        sceIoWrite(fd, header, sizeof(header)-1);
        sync_locked();
    }
}
void _log_print(int t, const char *fmt, ...) {
#ifdef ZOMBIE_RELEASE_BUILD
    if (t != LT_ERROR && t != LT_FATAL && t != LT_PERF) return;
#endif
    if (t < 0 || t > LT_PERF) return;
    uint64_t start = sceKernelGetProcessTimeWide();
    if (!lock_log()) return;
    open_locked();
    char message[2048], line[2200];
    va_list args; va_start(args, fmt);
    int length = sceClibVsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    int emit = 1;
    char repeat_suffix[48] = "";
#ifdef ZOMBIE_RELEASE_BUILD
    /* Do not rate-limit truncated messages: their differing tail is unknown. */
    if (t == LT_ERROR && length >= 0 && length < sizeof(message)) {
        unsigned slot;
        for (slot = 0; slot < 16; ++slot)
            if (repeats[slot].count && strcmp(message, repeats[slot].text) == 0) break;
        if (slot == 16) {
            slot = replacement++ % 16;
            strcpy(repeats[slot].text, message); repeats[slot].count = 0;
        }
        unsigned n = ++repeats[slot].count;
        if (n > 4 && (n & (n-1))) { emit = 0; stats.suppressed++; }
        else if (n > 4) {
            sceClibSnprintf(repeat_suffix, sizeof(repeat_suffix), " [repeat_count=%u]", n);
        }
    }
#endif
    if (emit) {
        static const char *tags[] = {"debug", "info", "warning", "error", "fatal", "success", "waiting", "PERF"};
        int n = sceClibSnprintf(line, sizeof(line), "[%s] %s%s\n", tags[t], message, repeat_suffix);
#ifdef DEBUG_SOLOADER
        sceClibPrintf("%s", line);
#else
        if (t == LT_FATAL) sceClibPrintf("%s", line);
#endif
        if (fd >= 0 && n > 0) {
            unsigned size = n >= sizeof(line) ? sizeof(line)-1 : (unsigned)n;
            if (buffered + size > sizeof(file_buffer)) flush_locked();
            if (buffered + size <= sizeof(file_buffer)) {
                memcpy(file_buffer + buffered, line, size); buffered += size;
                stats.lines++; unsynced++;
            }
        }
    }
    uint64_t now = sceKernelGetProcessTimeWide();
#ifdef DEBUG_SOLOADER
    if (t == LT_WARN || t == LT_ERROR || t == LT_FATAL || unsynced >= 32) sync_locked();
    else flush_locked(); /* Debug preserves every diagnostic before a crash. */
#else
    if (t == LT_FATAL || unsynced >= 64 || (unsynced && now-last_sync >= 1000000)) sync_locked();
#endif
    stats.total_us += (unsigned)(sceKernelGetProcessTimeWide()-start);
    sceKernelUnlockLwMutex(&mutex, 1);
}
