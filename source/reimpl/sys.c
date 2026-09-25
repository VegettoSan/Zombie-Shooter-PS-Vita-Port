/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/sys.h"

#include <sys/errno.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/clib.h>
#include <string.h>
#include <stdint.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/rtc.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

#include "utils/utils.h"
#include "utils/logger.h"
#include "utils/so_trace.h"

#define BIONIC_CLOCK_REALTIME           0
#define BIONIC_CLOCK_MONOTONIC          1
#define BIONIC_CLOCK_PROCESS_CPUTIME_ID 2
#define BIONIC_CLOCK_THREAD_CPUTIME_ID  3
#define BIONIC_CLOCK_MONOTONIC_RAW      4
#define BIONIC_CLOCK_REALTIME_COARSE    5
#define BIONIC_CLOCK_MONOTONIC_COARSE   6
#define BIONIC_CLOCK_BOOTTIME           7
#define BIONIC_CLOCK_REALTIME_ALARM     8
#define BIONIC_CLOCK_BOOTTIME_ALARM     9
#define BIONIC_CLOCK_SGI_CYCLE         10
#define BIONIC_CLOCK_TAI               11

// 1969 years in microseconds, used to adjust SCE tick to UNIX timestamp
#define __epoch 62135587294000000

int clock_gettime_soloader(clockid_t clock_id, struct timespec * tp) {
    switch (clock_id) {
        case BIONIC_CLOCK_MONOTONIC:
        case BIONIC_CLOCK_MONOTONIC_RAW:
        case BIONIC_CLOCK_MONOTONIC_COARSE:
        case BIONIC_CLOCK_BOOTTIME:
        case BIONIC_CLOCK_BOOTTIME_ALARM:
        case BIONIC_CLOCK_SGI_CYCLE:
        case BIONIC_CLOCK_PROCESS_CPUTIME_ID:
        case BIONIC_CLOCK_THREAD_CPUTIME_ID: {
            uint64_t proctime = sceKernelGetProcessTimeWide();

            tp->tv_sec = (proctime / 1000000);
            tp->tv_nsec = ((proctime - (tp->tv_sec * 1000000)) * 1000);
            break;
        }
        case BIONIC_CLOCK_REALTIME:
        case BIONIC_CLOCK_REALTIME_COARSE:
        case BIONIC_CLOCK_REALTIME_ALARM:
        case BIONIC_CLOCK_TAI: {
            SceRtcTick tick;
            sceRtcGetCurrentTick(&tick);
            tick.tick -= __epoch;

            tp->tv_sec = (tick.tick / 1000000);
            tp->tv_nsec = ((tick.tick - (tp->tv_sec * 1000000)) * 1000);
            break;
        }
        default:
            l_error("clock_gettime / unexpected clock id %i", clock_id);
    }

    return 0;
}

int clock_getres_soloader(clockid_t clock_id, struct timespec * res) {
    res->tv_sec = 0;
    res->tv_nsec = 1000;
    return 0;
}

clock_t clock_soloader(void) {
    return sceKernelGetProcessTimeLow();
}

/*
 * The loaded library uses Bionic's 32-bit ARM layout.  Passing that object to
 * VitaSDK's sigaction would reinterpret different flags/layout and would also
 * attempt to install an Android handler that expects a Linux ucontext.  Keep a
 * deterministic compatibility registry instead.  This is enough for the
 * game's CrashCatcher RAII object to save and restore its previous action.
 */
#define BIONIC_NSIG 32
_Static_assert(sizeof(bionic_sigaction) == 16,
               "Android ARM sigaction ABI must be 16 bytes");

static bionic_sigaction bionic_signal_actions[BIONIC_NSIG];
static bool bionic_signal_action_valid[BIONIC_NSIG];
static pthread_mutex_t bionic_signal_mutex = PTHREAD_MUTEX_INITIALIZER;

static void log_signal_address(const char *label, uintptr_t address) {
    uintptr_t offset = 0;
    if (so_trace_offset(address, &offset))
        l_info("[signal] %s=%p (so+0x%08X)", label, (void *)address,
               (unsigned)offset);
    else
        l_info("[signal] %s=%p (outside SO)", label, (void *)address);
}

int sigaction_soloader(int signum, const bionic_sigaction *act,
                       bionic_sigaction *oldact) {
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);

    l_info("[signal] sigaction(sig=%d, act=%p, oldact=%p)", signum,
           (const void *)act, (void *)oldact);
    log_signal_address("caller", caller);
    if (act) {
        log_signal_address("handler", act->handler);
        l_info("[signal] action mask=0x%08X flags=0x%08X restorer=%p",
               (unsigned)act->mask, (unsigned)act->flags,
               (void *)act->restorer);
    }

    if (signum <= 0 || signum >= BIONIC_NSIG) {
        errno = EINVAL;
        l_error("[signal] invalid Android signal number: %d", signum);
        return -1;
    }

    pthread_mutex_lock(&bionic_signal_mutex);
    if (oldact) {
        if (bionic_signal_action_valid[signum])
            *oldact = bionic_signal_actions[signum];
        else
            memset(oldact, 0, sizeof(*oldact)); /* SIG_DFL */
    }
    if (act) {
        bionic_signal_actions[signum] = *act;
        bionic_signal_action_valid[signum] = true;
    }
    pthread_mutex_unlock(&bionic_signal_mutex);

    return 0;
}

int pthread_sigmask_soloader(int how, const uint32_t *set, uint32_t *oldset) {
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    uintptr_t offset = 0;
    if (oldset)
        *oldset = 0;
    if (so_trace_offset(caller, &offset))
        l_info("[signal] pthread_sigmask(how=%d set=%p old=%p) caller=so+0x%08X",
               how, (const void *)set, (void *)oldset, (unsigned)offset);
    else
        l_info("[signal] pthread_sigmask(how=%d set=%p old=%p) caller=%p",
               how, (const void *)set, (void *)oldset, (void *)caller);
    return 0;
}

int __system_property_get_soloader(const char *name, char *value) {
    l_warn("__system_property_get(%s, %p): not implemented", name, value);
    strncpy(value, "psvita", 7);
    return 7;
}

void assert2(const char* f, int l, const char* func, const char* msg) {
    l_fatal("[%s:%i][%s] Assertion failed: %s", f, l, func, msg);
}

void syscall(int c) {
    l_warn("syscall(%i): not implemented", c);
}

void __stack_chk_fail_soloader() {
    l_fatal("Stack collapsed at address %p", __builtin_return_address(0));
}

void abort_soloader() {
    l_fatal("Abort called from address %p", __builtin_return_address(0));
    abort();
}

void exit_soloader(int status) {
    l_fatal("Exit(%i) called from %p", status, __builtin_return_address(0));
    exit(status);
}

int __atomic_dec(volatile int *ptr) {
    return __sync_fetch_and_sub(ptr, 1);
}

int __atomic_inc(volatile int *ptr) {
    return __sync_fetch_and_add(ptr, 1);
}

int __atomic_swap(int new_value, volatile int *ptr) {
    int old_value;
    do {
        old_value = *ptr;
    } while (__sync_val_compare_and_swap(ptr, old_value, new_value) != old_value);
    return old_value;
}

int __atomic_cmpxchg(int old_value, int new_value, volatile int* ptr) {
    /* We must return 0 on success */
    return __sync_val_compare_and_swap(ptr, old_value, new_value) != old_value;
}

char * getenv_soloader(const char * var) {
    l_warn("getenv(\"%s\"): not implemented.", var);
    return NULL;
}

int setenv_soloader(const char * name, const char * value, int overwrite) {
    l_warn("setenv(\"%s\", \"%s\"): not implemented.", name, value);
    return 0;
}

int getpagesize(void) {
    return PAGE_SIZE;
}
