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

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#ifndef NDK_PORT
#include "reimpl/controls.h"
#else
#include <falso_ndk/FalsoNDK.h>
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
static void breadcrumb(const char *msg) {
    char buf[256];
    int n = sceClibSnprintf(buf, sizeof(buf), "%s\n", msg);
    if (n > 0)
        file_save(DATA_PATH "ndk_step.txt", (const uint8_t *)buf, (size_t)n);
}

static void log_cb(const char *name, void *fn) {
    l_info("  %-28s = %p", name, fn);
}

static void log_activity_callbacks(ANativeActivity *activity) {
    if (!activity || !activity->callbacks) {
        l_warn("[ndk] activity or callbacks is NULL");
        return;
    }
    ANativeActivityCallbacks *c = activity->callbacks;
    l_info("[ndk] callbacks dump (sizeof callbacks=%u):",
           (unsigned)sizeof(ANativeActivityCallbacks));
    log_cb("onStart", (void *)c->onStart);
    log_cb("onResume", (void *)c->onResume);
    log_cb("onSaveInstanceState", (void *)c->onSaveInstanceState);
    log_cb("onPause", (void *)c->onPause);
    log_cb("onStop", (void *)c->onStop);
    log_cb("onDestroy", (void *)c->onDestroy);
    log_cb("onWindowFocusChanged", (void *)c->onWindowFocusChanged);
    log_cb("onNativeWindowCreated", (void *)c->onNativeWindowCreated);
    log_cb("onNativeWindowResized", (void *)c->onNativeWindowResized);
    log_cb("onNativeWindowRedrawNeeded", (void *)c->onNativeWindowRedrawNeeded);
    log_cb("onNativeWindowDestroyed", (void *)c->onNativeWindowDestroyed);
    log_cb("onInputQueueCreated", (void *)c->onInputQueueCreated);
    log_cb("onInputQueueDestroyed", (void *)c->onInputQueueDestroyed);
    log_cb("onContentRectChanged", (void *)c->onContentRectChanged);
    log_cb("onConfigurationChanged", (void *)c->onConfigurationChanged);
    log_cb("onLowMemory", (void *)c->onLowMemory);
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

    l_info("[ndk] sizeof(ANativeActivity)=%u sizeof(callbacks)=%u",
           (unsigned)sizeof(ANativeActivity),
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
    activity->externalDataPath = DATA_PATH "assets/";
    activity->sdkVersion = 19;
    activity->instance = NULL;
    activity->assetManager = NULL;
    activity->obbPath = DATA_PATH;

    l_info("[ndk] activity=%p callbacks=%p", (void *)activity, (void *)activity->callbacks);
    l_info("[ndk] internalDataPath=%s", activity->internalDataPath);
    l_info("[ndk] externalDataPath=%s", activity->externalDataPath);
    l_info("[ndk] sdkVersion=%d env=%p vm=%p", activity->sdkVersion,
           (void *)activity->env, (void *)activity->vm);

    ANativeActivity_createFunc *onCreate = (ANativeActivity_createFunc *)sym;

    breadcrumb("before_onCreate");
    l_info("[ndk] >>> ANativeActivity_onCreate @ 0x%08X",
           (unsigned)sym);
    onCreate(activity, NULL, 0);
    l_success("[ndk] <<< ANativeActivity_onCreate returned");
    breadcrumb("after_onCreate");

    /*
     * native_app_glue often starts android_main on another thread here.
     * Give it a moment; if that thread crashes, the coredump/log stop point
     * is clearer. Also write step file so we know onCreate finished even if
     * the process dies mid-log.
     */
    l_info("[ndk] waiting 3s for possible android_main thread...");
    sceKernelDelayThread(3 * 1000 * 1000);
    l_success("[ndk] still alive after 3s post-onCreate");
    breadcrumb("after_onCreate_wait");

    log_activity_callbacks(activity);
    breadcrumb("after_callbacks_dump");

    if (activity->callbacks->onStart) {
        breadcrumb("before_onStart");
        l_info("[ndk] >>> onStart @ %p", (void *)activity->callbacks->onStart);
        activity->callbacks->onStart(activity);
        l_success("[ndk] <<< onStart OK");
        breadcrumb("after_onStart");
    } else {
        l_warn("[ndk] onStart is NULL — skip");
    }

    if (activity->callbacks->onResume) {
        breadcrumb("before_onResume");
        l_info("[ndk] >>> onResume @ %p", (void *)activity->callbacks->onResume);
        activity->callbacks->onResume(activity);
        l_success("[ndk] <<< onResume OK");
        breadcrumb("after_onResume");
    } else {
        l_warn("[ndk] onResume is NULL — skip");
    }

    if (activity->callbacks->onInputQueueCreated) {
        breadcrumb("before_input_queue");
        l_info("[ndk] AInputQueue_create...");
        AInputQueue *input_queue = AInputQueue_create();
        l_info("[ndk] input_queue=%p >>> onInputQueueCreated", (void *)input_queue);
        activity->callbacks->onInputQueueCreated(activity, input_queue);
        l_success("[ndk] <<< onInputQueueCreated OK");
        breadcrumb("after_input_queue");
    } else {
        l_warn("[ndk] onInputQueueCreated is NULL — skip");
    }

    ANativeWindow *window = NULL;
    if (activity->callbacks->onNativeWindowCreated) {
        breadcrumb("before_window_created");
        l_info("[ndk] ANativeWindow_create...");
        window = ANativeWindow_create();
        l_info("[ndk] window=%p >>> onNativeWindowCreated", (void *)window);
        activity->callbacks->onNativeWindowCreated(activity, window);
        l_success("[ndk] <<< onNativeWindowCreated OK");
        breadcrumb("after_window_created");
    } else {
        l_warn("[ndk] onNativeWindowCreated is NULL — skip");
    }

    if (activity->callbacks->onNativeWindowResized && window) {
        breadcrumb("before_window_resized");
        l_info("[ndk] >>> onNativeWindowResized");
        activity->callbacks->onNativeWindowResized(activity, window);
        l_success("[ndk] <<< onNativeWindowResized OK");
    }

    if (activity->callbacks->onWindowFocusChanged) {
        breadcrumb("before_focus");
        l_info("[ndk] >>> onWindowFocusChanged(1)");
        activity->callbacks->onWindowFocusChanged(activity, 1);
        l_success("[ndk] <<< onWindowFocusChanged OK");
        breadcrumb("after_focus");
    } else {
        l_warn("[ndk] onWindowFocusChanged is NULL — skip");
    }

    if (activity->callbacks->onNativeWindowRedrawNeeded && window) {
        breadcrumb("before_redraw");
        l_info("[ndk] >>> onNativeWindowRedrawNeeded");
        activity->callbacks->onNativeWindowRedrawNeeded(activity, window);
        l_success("[ndk] <<< onNativeWindowRedrawNeeded OK");
    }

    l_success("[ndk] lifecycle sequence finished — idle loop");
    breadcrumb("idle_loop");

    unsigned frame = 0;
    while (1) {
        if (frame == 0)
            l_info("[ndk] first idle frame");
        else if ((frame % 300) == 0)
            l_info("[ndk] idle alive frame=%u", frame);

        gl_swap();
        frame++;
        sceKernelDelayThread(8000);
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
