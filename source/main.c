/*
 * Zombie Shooter Vita — main entry
 *
 * NativeActivity path: ANativeActivity_onCreate is the public entry.
 * native_app_glue typically spawns android_main on a *second* thread inside
 * onCreate; that thread can crash independently while we still log on this one.
 */

#include "utils/init.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/dialog.h"
#include "utils/utils.h"
#include "utils/so_trace.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <stddef.h>

#ifndef NDK_PORT
#include "reimpl/controls.h"
#else
#include <falso_ndk/FalsoNDK.h>
#endif

#ifdef NDK_PORT
/* Keep FalsoNDK diagnostics in the same crash-safe persistent log. */
void fndk_log(int severity, const char *message) {
    if (!message) return;
    switch (severity) {
        case FALSONDK_LOG_FATAL:
        case FALSONDK_LOG_ERROR:
            l_error("[FalsoNDK] %s", message);
            break;
        case FALSONDK_LOG_WARN:
            l_warn("[FalsoNDK] %s", message);
            break;
        default:
            l_debug("[FalsoNDK] %s", message);
            break;
    }
}
#endif

int _newlib_heap_size_user = 256 * 1024 * 1024;

#ifdef USE_SCELIBC_IO
int sceLibcHeapSize = 4 * 1024 * 1024;
#endif

so_module so_mod;

static uintptr_t find_so_symbol(const char *name) {
    uintptr_t addr = so_symbol(&so_mod, name);
    if (addr)
        l_success("symbol \"%s\" FOUND at 0x%08X", name, (unsigned)addr);
    else
        l_warn("symbol \"%s\" NOT FOUND", name);
    return addr;
}

#ifdef NDK_PORT
_Static_assert(sizeof(ANativeActivity) == 40,
               "ANativeActivity must match Android ARM32 ABI");
_Static_assert(sizeof(ANativeActivityCallbacks) == 64,
               "ANativeActivityCallbacks must match Android ARM32 ABI");

static void breadcrumb(const char *msg) {
    char buf[256];
    int n = sceClibSnprintf(buf, sizeof(buf), "%s\n", msg);
    if (n > 0)
        file_save(DATA_PATH "ndk_step.txt", (const uint8_t *)buf, (size_t)n);
}

static bool valid_so_callback(const char *name, const void *fn) {
    uintptr_t address = (uintptr_t)fn;
    uintptr_t offset = 0;
    if (!address) {
        l_warn("[ndk] callback %-28s = NULL", name);
        return false;
    }
    if (!so_trace_is_code(address) || !so_trace_offset(address, &offset)) {
        l_error("[ndk] callback %-28s = %p INVALID (outside SO text)",
                name, fn);
        return false;
    }
    l_info("[ndk] callback %-28s = %p (so+0x%08X) valid",
           name, fn, (unsigned)offset);
    return true;
}

static void log_activity_layout(void) {
    l_info("[ndk] ANativeActivity size=%u offsets: callbacks=%u vm=%u env=%u clazz=%u",
           (unsigned)sizeof(ANativeActivity),
           (unsigned)offsetof(ANativeActivity, callbacks),
           (unsigned)offsetof(ANativeActivity, vm),
           (unsigned)offsetof(ANativeActivity, env),
           (unsigned)offsetof(ANativeActivity, clazz));
    l_info("[ndk] activity offsets: internal=%u external=%u sdk=%u instance=%u assets=%u obb=%u",
           (unsigned)offsetof(ANativeActivity, internalDataPath),
           (unsigned)offsetof(ANativeActivity, externalDataPath),
           (unsigned)offsetof(ANativeActivity, sdkVersion),
           (unsigned)offsetof(ANativeActivity, instance),
           (unsigned)offsetof(ANativeActivity, assetManager),
           (unsigned)offsetof(ANativeActivity, obbPath));
}

static void log_activity_callbacks(ANativeActivity *activity) {
    if (!activity || !activity->callbacks) {
        l_warn("[ndk] activity or callbacks is NULL");
        return;
    }
    ANativeActivityCallbacks *c = activity->callbacks;
    l_info("[ndk] callbacks dump (sizeof callbacks=%u):",
           (unsigned)sizeof(ANativeActivityCallbacks));
    valid_so_callback("onStart", (void *)c->onStart);
    valid_so_callback("onResume", (void *)c->onResume);
    valid_so_callback("onSaveInstanceState", (void *)c->onSaveInstanceState);
    valid_so_callback("onPause", (void *)c->onPause);
    valid_so_callback("onStop", (void *)c->onStop);
    valid_so_callback("onDestroy", (void *)c->onDestroy);
    valid_so_callback("onWindowFocusChanged", (void *)c->onWindowFocusChanged);
    valid_so_callback("onNativeWindowCreated", (void *)c->onNativeWindowCreated);
    valid_so_callback("onNativeWindowResized", (void *)c->onNativeWindowResized);
    valid_so_callback("onNativeWindowRedrawNeeded", (void *)c->onNativeWindowRedrawNeeded);
    valid_so_callback("onNativeWindowDestroyed", (void *)c->onNativeWindowDestroyed);
    valid_so_callback("onInputQueueCreated", (void *)c->onInputQueueCreated);
    valid_so_callback("onInputQueueDestroyed", (void *)c->onInputQueueDestroyed);
    valid_so_callback("onContentRectChanged", (void *)c->onContentRectChanged);
    valid_so_callback("onConfigurationChanged", (void *)c->onConfigurationChanged);
    valid_so_callback("onLowMemory", (void *)c->onLowMemory);
}

static void *ndk_game_thread(void *arg) {
    (void)arg;

    l_info("[ndk] game thread start");
    breadcrumb("game_thread_start");

    uintptr_t sym = find_so_symbol("ANativeActivity_onCreate");
    if (!sym) {
        l_fatal("[ndk] ANativeActivity_onCreate missing");
        fatal_error("ANativeActivity_onCreate not found in libzombie_shooter.so");
        return NULL;
    }

    find_so_symbol("android_main");
    find_so_symbol("JNI_OnLoad");

    log_activity_layout();
    l_info("[ndk] ANativeActivityCallbacks size=%u (16 ARM32 pointers)",
           (unsigned)sizeof(ANativeActivityCallbacks));

    ANativeActivity *activity = (ANativeActivity *)calloc(1, sizeof(ANativeActivity));
    if (!activity) {
        l_fatal("[ndk] calloc(ANativeActivity) failed");
        fatal_error("Out of memory (ANativeActivity)");
        return NULL;
    }

    activity->callbacks = (ANativeActivityCallbacks *)calloc(1, sizeof(ANativeActivityCallbacks));
    if (!activity->callbacks) {
        l_fatal("[ndk] calloc(ANativeActivityCallbacks) failed");
        fatal_error("Out of memory (ANativeActivityCallbacks)");
        return NULL;
    }

    activity->env = &jni;
    activity->vm = &jvm;
    activity->clazz = (jclass)0x42424242;
    activity->internalDataPath = DATA_PATH;
    activity->externalDataPath = DATA_PATH;
    activity->sdkVersion = 24;
    activity->instance = NULL;
    activity->assetManager = AAssetManager_create();
    activity->obbPath = DATA_PATH;

    if (!activity->assetManager) {
        l_fatal("[ndk] AAssetManager_create failed");
        fatal_error("Out of memory (AAssetManager)");
        return NULL;
    }

    l_info("[ndk] activity=%p callbacks=%p", (void *)activity, (void *)activity->callbacks);
    l_info("[ndk] internalDataPath=%s", activity->internalDataPath);
    l_info("[ndk] externalDataPath=%s", activity->externalDataPath);
    l_info("[ndk] sdkVersion=%d env=%p vm=%p assetManager=%p",
           activity->sdkVersion, (void *)activity->env, (void *)activity->vm,
           (void *)activity->assetManager);

    ANativeActivity_createFunc *onCreate = (ANativeActivity_createFunc *)sym;

    breadcrumb("before_onCreate");
    l_info("[ndk] >>> ANativeActivity_onCreate @ 0x%08X",
           (unsigned)sym);
    onCreate(activity, NULL, 0);
    l_success("[ndk] <<< ANativeActivity_onCreate returned");
    l_info("[ndk] post-onCreate instance=%p callbacks=%p",
           activity->instance, (void *)activity->callbacks);
    breadcrumb("after_onCreate");

    /* Android dispatches lifecycle events as soon as onCreate returns.  The
     * previous diagnostic delay let the game's worker finish/quit before it
     * ever received APP_CMD_START, RESUME, INPUT_CHANGED or INIT_WINDOW. */
    l_info("[ndk] dispatching lifecycle immediately after onCreate");
    log_activity_callbacks(activity);
    breadcrumb("after_callbacks_dump");

    if (valid_so_callback("onStart/invoke", (void *)activity->callbacks->onStart)) {
        breadcrumb("before_onStart");
        l_info("[ndk] >>> onStart @ %p", (void *)activity->callbacks->onStart);
        activity->callbacks->onStart(activity);
        l_success("[ndk] <<< onStart OK");
        breadcrumb("after_onStart");
    } else {
        l_warn("[ndk] onStart is NULL — skip");
    }

    if (valid_so_callback("onResume/invoke", (void *)activity->callbacks->onResume)) {
        breadcrumb("before_onResume");
        l_info("[ndk] >>> onResume @ %p", (void *)activity->callbacks->onResume);
        activity->callbacks->onResume(activity);
        l_success("[ndk] <<< onResume OK");
        breadcrumb("after_onResume");
    } else {
        l_warn("[ndk] onResume is NULL — skip");
    }

    if (valid_so_callback("onInputQueueCreated/invoke",
                          (void *)activity->callbacks->onInputQueueCreated)) {
        breadcrumb("before_input_queue");
        l_info("[ndk] AInputQueue_create...");
        AInputQueue *input_queue = AInputQueue_create();
        if (input_queue) {
            l_info("[ndk] input_queue=%p >>> onInputQueueCreated", (void *)input_queue);
            activity->callbacks->onInputQueueCreated(activity, input_queue);
            l_success("[ndk] <<< onInputQueueCreated OK");
            breadcrumb("after_input_queue");
        } else {
            l_error("[ndk] AInputQueue_create returned NULL — callback skipped");
            breadcrumb("input_queue_null");
        }
    } else {
        l_warn("[ndk] onInputQueueCreated is NULL — skip");
    }

    ANativeWindow *window = NULL;
    if (valid_so_callback("onNativeWindowCreated/invoke",
                          (void *)activity->callbacks->onNativeWindowCreated)) {
        breadcrumb("before_window_created");
        l_info("[ndk] ANativeWindow_create...");
        window = ANativeWindow_create();
        if (window) {
            l_info("[ndk] window=%p width=%d height=%d format=%d",
                   (void *)window, ANativeWindow_getWidth(window),
                   ANativeWindow_getHeight(window),
                   ANativeWindow_getFormat(window));
            l_info("[ndk] >>> onNativeWindowCreated");
            activity->callbacks->onNativeWindowCreated(activity, window);
            l_success("[ndk] <<< onNativeWindowCreated OK");
            breadcrumb("after_window_created");
        } else {
            l_error("[ndk] ANativeWindow_create returned NULL — callback skipped");
            breadcrumb("window_null");
        }
    } else {
        l_warn("[ndk] onNativeWindowCreated is NULL — skip");
    }

    if (window && valid_so_callback("onNativeWindowResized/invoke",
                                    (void *)activity->callbacks->onNativeWindowResized)) {
        breadcrumb("before_window_resized");
        l_info("[ndk] >>> onNativeWindowResized");
        activity->callbacks->onNativeWindowResized(activity, window);
        l_success("[ndk] <<< onNativeWindowResized OK");
    }

    if (valid_so_callback("onWindowFocusChanged/invoke",
                          (void *)activity->callbacks->onWindowFocusChanged)) {
        breadcrumb("before_focus");
        l_info("[ndk] >>> onWindowFocusChanged(1)");
        activity->callbacks->onWindowFocusChanged(activity, 1);
        l_success("[ndk] <<< onWindowFocusChanged OK");
        breadcrumb("after_focus");
    } else {
        l_warn("[ndk] onWindowFocusChanged is NULL — skip");
    }

    if (window && valid_so_callback("onNativeWindowRedrawNeeded/invoke",
                                    (void *)activity->callbacks->onNativeWindowRedrawNeeded)) {
        breadcrumb("before_redraw");
        l_info("[ndk] >>> onNativeWindowRedrawNeeded");
        activity->callbacks->onNativeWindowRedrawNeeded(activity, window);
        l_success("[ndk] <<< onNativeWindowRedrawNeeded OK");
    }

    l_success("[ndk] lifecycle sequence finished — idle loop");
    breadcrumb("idle_loop");

    unsigned heartbeat = 0;
    unsigned previous_sync_count = 0;
    unsigned previous_sync_us = 0;
    while (1) {
        if ((heartbeat % 50) == 0) {
            unsigned presents = egl_present_count();
            unsigned age_ms = egl_present_age_ms();
            unsigned sync_count, sync_us;
            logger_get_sync_stats(&sync_count, &sync_us);
            l_info("[PERF] lifecycle heartbeat=%u presents=%u last_present_age_ms=%u log_syncs=%u log_sync_ms=%u",
                   heartbeat, presents, age_ms,
                   sync_count - previous_sync_count,
                   (sync_us - previous_sync_us) / 1000);
            previous_sync_count = sync_count;
            previous_sync_us = sync_us;
#if defined(ZOMBIE_STALL_DUMP)
            if (presents >= 1000 && age_ms >= 45000) {
                l_fatal("[CRASH] diagnostic stall dump: presents=%u age_ms=%u (Debug only)",
                        presents, age_ms);
                abort();
            }
#endif
        }

        /* The game's render thread presents through eglSwapBuffers. */
        heartbeat++;
        sceKernelDelayThread(100000);
    }

    return NULL;
}

static void run_ndk_path(void) {
    static const size_t stacks[] = {
        2 * 1024 * 1024,
        1 * 1024 * 1024,
        512 * 1024,
    };

    for (unsigned i = 0; i < sizeof(stacks) / sizeof(stacks[0]); i++) {
        pthread_t t;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, stacks[i]);

        l_info("[main] pthread_create try stack=%u KB...", (unsigned)(stacks[i] / 1024));
        int pret = pthread_create(&t, &attr, ndk_game_thread, NULL);
        pthread_attr_destroy(&attr);

        if (pret == 0) {
            l_success("[main] NDK thread OK (stack=%u KB) — joining", (unsigned)(stacks[i] / 1024));
            pthread_join(t, NULL);
            l_warn("[main] NDK thread exited (unexpected)");
            return;
        }

        l_warn("[main] pthread_create failed: %d (errno=%d) stack=%u KB",
               pret, errno, (unsigned)(stacks[i] / 1024));
    }

    l_warn("[main] all pthread attempts failed — running NDK path on MAIN thread");
    ndk_game_thread(NULL);
}
#endif /* NDK_PORT */

int main() {
    l_info("=== main() start ===");
#ifdef NDK_PORT
    l_info("[main] build: NDK_PORT=ON (NativeActivity)");
#else
    l_info("[main] build: NDK_PORT=OFF (JNI)");
#endif

    l_info("[main] calling soloader_init_all()...");
    soloader_init_all();
    l_success("[main] soloader_init_all() returned");

    l_info("[main] calling gl_init()...");
    gl_init();
    l_success("[main] gl_init() returned");

#ifdef NDK_PORT
    run_ndk_path();
#else
    l_info("[main] probing JNI symbols...");
    uintptr_t sym_jni = find_so_symbol("JNI_OnLoad");
    if (sym_jni) {
        int (*JNI_OnLoad_fn)(void *jvm) = (void *)sym_jni;
        l_info("[main] >>> JNI_OnLoad");
        int ver = JNI_OnLoad_fn(&jvm);
        l_success("[main] <<< JNI_OnLoad returned %d", ver);
    }

    l_info("[main] non-NDK idle loop");
    unsigned frame = 0;
    while (1) {
        if ((frame % 300) == 0)
            l_info("[main] idle frame=%u", frame);
        gl_swap();
        frame++;
        sceKernelDelayThread(8000);
    }
#endif

    sceKernelExitDeleteThread(0);
    return 0;
}

#ifndef NDK_PORT
void controls_handler_key(int32_t keycode, ControlsAction action) {
    l_debug("[controls] key code=%d action=%d", (int)keycode, (int)action);
}

void controls_handler_touch(int32_t id, float x, float y, ControlsAction action) {
    l_debug("[controls] touch id=%d x=%.1f y=%.1f action=%d",
            (int)id, x, y, (int)action);
}

void controls_handler_analog(ControlsStickId which, float x, float y, ControlsAction action) {
    l_debug("[controls] analog stick=%d x=%.2f y=%.2f action=%d",
            (int)which, x, y, (int)action);
}
#endif
