/*
 * Zombie Shooter Vita — main entry
 *
 * The Android .so exports ANativeActivity_onCreate (NativeActivity / NDK).
 * It does NOT export JNI_OnLoad or android_main as a public dynamic symbol
 * (android_main is usually internal to native_app_glue and started from
 * ANativeActivity_onCreate on a secondary thread).
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
/* Dump which lifecycle callbacks the game filled in after onCreate. */
static void log_activity_callbacks(ANativeActivity *activity) {
    if (!activity || !activity->callbacks) {
        l_warn("[ndk] activity or callbacks is NULL");
        return;
    }
    ANativeActivityCallbacks *c = activity->callbacks;
    l_info("[ndk] callbacks dump:");
    l_info("  onStart                 = %p", (void *)c->onStart);
    l_info("  onResume                = %p", (void *)c->onResume);
    l_info("  onPause                 = %p", (void *)c->onPause);
    l_info("  onStop                  = %p", (void *)c->onStop);
    l_info("  onDestroy               = %p", (void *)c->onDestroy);
    l_info("  onWindowFocusChanged    = %p", (void *)c->onWindowFocusChanged);
    l_info("  onNativeWindowCreated   = %p", (void *)c->onNativeWindowCreated);
    l_info("  onNativeWindowResized   = %p", (void *)c->onNativeWindowResized);
    l_info("  onNativeWindowRedrawNeeded = %p", (void *)c->onNativeWindowRedrawNeeded);
    l_info("  onNativeWindowDestroyed = %p", (void *)c->onNativeWindowDestroyed);
    l_info("  onInputQueueCreated     = %p", (void *)c->onInputQueueCreated);
    l_info("  onInputQueueDestroyed   = %p", (void *)c->onInputQueueDestroyed);
    l_info("  onContentRectChanged    = %p", (void *)c->onContentRectChanged);
    l_info("  onConfigurationChanged  = %p", (void *)c->onConfigurationChanged);
    l_info("  onLowMemory             = %p", (void *)c->onLowMemory);
}

typedef void (*ANativeActivity_createFunc)(ANativeActivity *activity,
                                           void *savedState,
                                           size_t savedStateSize);

/* Run ANativeActivity_onCreate + full lifecycle on a thread with a large stack
 * (same idea as mc3-vita game_thread). native_app_glue often needs it. */
static void *ndk_game_thread(void *arg) {
    (void)arg;

    l_info("[ndk] game thread start");

    uintptr_t sym = find_so_symbol("ANativeActivity_onCreate");
    if (!sym) {
        l_fatal("[ndk] ANativeActivity_onCreate missing");
        fatal_error("ANativeActivity_onCreate not found in libzombie_shooter.so");
        return NULL;
    }

    /* Optional: some builds also export android_main as dynamic — log only */
    find_so_symbol("android_main");
    find_so_symbol("JNI_OnLoad");

    l_info("[ndk] allocating ANativeActivity (%u bytes)...",
           (unsigned)sizeof(ANativeActivity));
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
    /* Saves / private files */
    activity->internalDataPath = DATA_PATH;
    /* Shared / assets root (APK assets extracted here) */
    activity->externalDataPath = DATA_PATH "assets/";
    activity->sdkVersion = 19; /* KitKat — matches java.c SDK_INT */
    activity->instance = NULL;
    activity->assetManager = NULL; /* FalsoNDK may fill later via JNI */

    l_info("[ndk] activity=%p callbacks=%p", (void *)activity, (void *)activity->callbacks);
    l_info("[ndk] internalDataPath=%s", activity->internalDataPath);
    l_info("[ndk] externalDataPath=%s", activity->externalDataPath);
    l_info("[ndk] sdkVersion=%d env=%p vm=%p", activity->sdkVersion,
           (void *)activity->env, (void *)activity->vm);

    ANativeActivity_createFunc onCreate = (ANativeActivity_createFunc)sym;

    l_info("[ndk] >>> ANativeActivity_onCreate(activity, NULL, 0) @ 0x%08X",
           (unsigned)sym);
    onCreate(activity, NULL, 0);
    l_success("[ndk] <<< ANativeActivity_onCreate returned");

    log_activity_callbacks(activity);

    /* Drive lifecycle — each step logged; null-safe */
    if (activity->callbacks->onStart) {
        l_info("[ndk] >>> onStart");
        activity->callbacks->onStart(activity);
        l_success("[ndk] <<< onStart OK");
    } else {
        l_warn("[ndk] onStart is NULL — skip");
    }

    if (activity->callbacks->onResume) {
        l_info("[ndk] >>> onResume");
        activity->callbacks->onResume(activity);
        l_success("[ndk] <<< onResume OK");
    } else {
        l_warn("[ndk] onResume is NULL — skip");
    }

    AInputQueue *input_queue = NULL;
    if (activity->callbacks->onInputQueueCreated) {
        l_info("[ndk] AInputQueue_create...");
        input_queue = AInputQueue_create();
        l_info("[ndk] input_queue=%p >>> onInputQueueCreated", (void *)input_queue);
        activity->callbacks->onInputQueueCreated(activity, input_queue);
        l_success("[ndk] <<< onInputQueueCreated OK");
    } else {
        l_warn("[ndk] onInputQueueCreated is NULL — skip");
    }

    ANativeWindow *window = NULL;
    if (activity->callbacks->onNativeWindowCreated) {
        l_info("[ndk] ANativeWindow_create...");
        window = ANativeWindow_create();
        l_info("[ndk] window=%p >>> onNativeWindowCreated", (void *)window);
        activity->callbacks->onNativeWindowCreated(activity, window);
        l_success("[ndk] <<< onNativeWindowCreated OK");
    } else {
        l_warn("[ndk] onNativeWindowCreated is NULL — skip");
    }

    if (activity->callbacks->onNativeWindowResized && window) {
        l_info("[ndk] >>> onNativeWindowResized");
        activity->callbacks->onNativeWindowResized(activity, window);
        l_success("[ndk] <<< onNativeWindowResized OK");
    }

    if (activity->callbacks->onWindowFocusChanged) {
        l_info("[ndk] >>> onWindowFocusChanged(1)");
        activity->callbacks->onWindowFocusChanged(activity, 1);
        l_success("[ndk] <<< onWindowFocusChanged OK");
    } else {
        l_warn("[ndk] onWindowFocusChanged is NULL — skip");
    }

    if (activity->callbacks->onNativeWindowRedrawNeeded && window) {
        l_info("[ndk] >>> onNativeWindowRedrawNeeded");
        activity->callbacks->onNativeWindowRedrawNeeded(activity, window);
        l_success("[ndk] <<< onNativeWindowRedrawNeeded OK");
    }

    l_success("[ndk] lifecycle sequence finished — idle loop (gl_swap)");

    /*
     * native_app_glue games usually run their own loop on another thread
     * started inside onCreate. We keep the process alive and present frames.
     */
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
    /*
     * Large stack like mc3-vita — ANativeActivity_onCreate / native_app_glue
     * can blow a default stack.
     */
    l_info("[main] spawning NDK game thread (stack=8MB)...");
    pthread_t t;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8 * 1024 * 1024);
    int pret = pthread_create(&t, &attr, ndk_game_thread, NULL);
    pthread_attr_destroy(&attr);
    if (pret != 0) {
        l_fatal("[main] pthread_create failed: %d", pret);
        fatal_error("pthread_create failed (%d)", pret);
    }
    l_success("[main] NDK game thread created — joining");
    pthread_join(t, NULL);
    l_warn("[main] NDK game thread exited (unexpected)");
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
