/*
 * FalsoNDK logging bridge for Zombie Shooter Vita.
 *
 * FalsoNDK's default weak fndk_log() prints every ALOGD/ALOGW/ALOGE call
 * through sceClibPrintf.  That is useful while bringing the port up, but it
 * becomes a very expensive hot path while the game is loading thousands of
 * assets.  Keep the full stream in Debug and make Release effectively silent
 * except for fatal messages (which are immediately followed by abort anyway).
 */

#include <psp2/kernel/clib.h>

/* Keep these values in sync with FalsoNDK_Utils.h. */
#define FALSONDK_LOG_DEBUG 0
#define FALSONDK_LOG_WARN  1
#define FALSONDK_LOG_ERROR 2
#define FALSONDK_LOG_FATAL 3

void fndk_log(int severity, const char *message) {
    if (!message)
        return;

#ifndef NDEBUG
    static const char *const prefixes[] = {"D", "W", "E", "F"};
    const char *prefix = (severity >= FALSONDK_LOG_DEBUG &&
                          severity <= FALSONDK_LOG_FATAL)
                             ? prefixes[severity]
                             : "?";
    sceClibPrintf("%s/FalsoNDK: %s\n", prefix, message);
#else
    /* Release is for gameplay/performance measurements.  Debug retains the
     * complete FalsoNDK diagnostic stream. */
    if (severity >= FALSONDK_LOG_FATAL)
        sceClibPrintf("F/FalsoNDK: %s\n", message);
#endif
}
