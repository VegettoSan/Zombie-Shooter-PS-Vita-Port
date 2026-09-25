/*
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  logger.h
 * @brief Logging utilities.
 */

#ifndef SOLOADER_LOGGER_H
#define SOLOADER_LOGGER_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LT_DEBUG   0
#define LT_INFO    1
#define LT_WARN    2
#define LT_ERROR   3
#define LT_FATAL   4
#define LT_SUCCESS 5
#define LT_WAIT    6

/*
 * Do not infer the gameplay/diagnostic variant from NDEBUG. VitaSDK/CMake
 * toolchains are free to choose their own Release flags, and run #8 proved
 * that relying on NDEBUG did not give us a trustworthy hardware identity.
 *
 * CMake now defines exactly one of:
 *   ZOMBIE_DEBUG_BUILD=1
 *   ZOMBIE_RELEASE_BUILD=1
 *
 * Debug keeps the full bring-up trace. Release keeps only aggregated [PERF]
 * info plus errors/fatals. This is the contract used for real-Vita A/B tests.
 */
#if defined(DEBUG_SOLOADER) && defined(ZOMBIE_DEBUG_BUILD)
#define l_debug(...)   _log_print(LT_DEBUG,   __VA_ARGS__)
#define l_info(...)    _log_print(LT_INFO,    __VA_ARGS__)
#define l_warn(...)    _log_print(LT_WARN,    __VA_ARGS__)
#define l_success(...) _log_print(LT_SUCCESS, __VA_ARGS__)
#define l_wait(...)    _log_print(LT_WAIT,    __VA_ARGS__)
#elif defined(DEBUG_SOLOADER) && defined(ZOMBIE_RELEASE_BUILD)
#define ZOMBIE_RELEASE_KEEP_INFO(fmt) \
    ((fmt) != NULL && strncmp((fmt), "[PERF]", 6) == 0)
#define l_debug(...)
#define l_info(fmt, ...) \
    do { \
        if (ZOMBIE_RELEASE_KEEP_INFO(fmt)) \
            _log_print(LT_INFO, (fmt), ##__VA_ARGS__); \
    } while (0)
#define l_warn(...)
#define l_success(...)
#define l_wait(...)
#else
#define l_debug(...)
#define l_info(...)
#define l_warn(...)
#define l_success(...)
#define l_wait(...)
#endif

/* Explicit performance channel: never depend on generic Release filtering. */
#define l_perf(...)    _log_print(LT_INFO,    __VA_ARGS__)

/* Errors and fatals are retained in every configuration. */
#define l_error(...)   _log_print(LT_ERROR,   __VA_ARGS__)
#define l_fatal(...)   _log_print(LT_FATAL,   __VA_ARGS__)

void _log_print(int t, const char* fmt, ...)
                __attribute__ ((format (printf, 2, 3)));
void logger_get_sync_stats(unsigned *count, unsigned *microseconds);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_LOGGER_H
