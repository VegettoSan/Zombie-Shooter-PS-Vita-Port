#include "utils/init.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/dialog.h"
#include "utils/utils.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#include <string.h>

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

/* Try several common Android native entry symbols and log what exists. */
static uintptr_t find_so_symbol(const char *name) {
    uintptr_t addr = so_symbol(&so_mod, name);
    if (addr)
        l_success("symbol \"%s\" FOUND at 0x%08X", name, (unsigned)addr);
    else
        l_warn("symbol \"%s\" NOT FOUND", name);
    return addr;
}

int main() {
    l_info("=== main() start ===");

    l_info("[main] calling soloader_init_all()...");
    soloader_init_all();
    l_success("[main] soloader_init_all() returned");

    /* ---- Probe common entrypoints ---- */
    l_info("[main] probing native symbols...");
    uintptr_t sym_jni_onload = find_so_symbol("JNI_OnLoad");
    uintptr_t sym_android_main = find_so_symbol("android_main");
    uintptr_t sym_ANativeActivity_onCreate = find_so_symbol("ANativeActivity_onCreate");
    /* Some games use mangled / alternate names */
    uintptr_t sym_JNI_OnLoad_alt = find_so_symbol("Java_com_android_..."); /* unlikely; just probe pattern */
    (void)sym_JNI_OnLoad_alt;

    /* ---- JNI_OnLoad ---- */
    if (sym_jni_onload) {
        int (*JNI_OnLoad_fn)(void *jvm) = (void *)sym_jni_onload;
        l_info("[main] calling JNI_OnLoad(&jvm) @ 0x%08X ...", (unsigned)sym_jni_onload);
        int jni_ver = JNI_OnLoad_fn(&jvm);
        l_success("[main] JNI_OnLoad returned %d (0x%X)", jni_ver, (unsigned)jni_ver);
    } else {
        l_warn("[main] JNI_OnLoad missing — skipping");
    }

    /* ---- OpenGL full init ---- */
    l_info("[main] calling gl_init()...");
    gl_init();
    l_success("[main] gl_init() returned");

#ifndef NDK_PORT
    l_info("[main] non-NDK path: entering render loop");

    /*
     * Until we wire real game callbacks, keep a safe loop so we can confirm
     * we reached this point. Log every N frames so the log file grows slowly
     * even if the friend leaves it running.
     */
    unsigned frame = 0;
    while (1) {
        if (frame == 0)
            l_info("[main] first frame / gl_swap()");
        else if ((frame % 120) == 0)
            l_info("[main] still alive, frame=%u", frame);

        /* TODO: call into game render / update once we know the entry */
        gl_swap();
        frame++;

        /* Yield a bit so we don't starve the system */
        sceKernelDelayThread(8000); /* ~8ms */
    }
#else
    l_info("[main] NDK_PORT path");

    if (!sym_ANativeActivity_onCreate) {
        l_fatal("[main] ANativeActivity_onCreate not found — cannot start NDK game");
        fatal_error("ANativeActivity_onCreate not found in libzombie_shooter.so");
    }

    l_info("[main] allocating ANativeActivity...");
    ANativeActivity *activity = malloc(sizeof(ANativeActivity));
    if (!activity) {
        l_fatal("[main] malloc(ANativeActivity) failed");
        fatal_error("Out of memory (ANativeActivity)");
    }
    memset(activity, 0, sizeof(*activity));

    activity->callbacks = malloc(sizeof(ANativeActivityCallbacks));
    if (!activity->callbacks) {
        l_fatal("[main] malloc(ANativeActivityCallbacks) failed");
        fatal_error("Out of memory (ANativeActivityCallbacks)");
    }
    memset(activity->callbacks, 0, sizeof(ANativeActivityCallbacks));

    activity->env = &jni;
    activity->vm = &jvm;
    activity->clazz = (jclass)0x42424242;
    activity->internalDataPath = DATA_PATH "assets/";
    activity->externalDataPath = DATA_PATH "assets/";
    activity->sdkVersion = 14;
    activity->instance = NULL;

    l_info("[main] internalDataPath=%s", activity->internalDataPath);
    l_info("[main] externalDataPath=%s", activity->externalDataPath);

    int (*ANativeActivity_onCreate)(ANativeActivity *, void *, size_t) =
        (void *)sym_ANativeActivity_onCreate;

    l_info("[main] calling ANativeActivity_onCreate...");
    ANativeActivity_onCreate(activity, NULL, 0);
    l_success("[main] ANativeActivity_onCreate returned");

    if (activity->callbacks->onStart) {
        l_info("[main] onStart...");
        activity->callbacks->onStart(activity);
        l_success("[main] onStart OK");
    } else {
        l_warn("[main] onStart callback is NULL");
    }

    if (activity->callbacks->onResume) {
        l_info("[main] onResume...");
        activity->callbacks->onResume(activity);
        l_success("[main] onResume OK");
    } else {
        l_warn("[main] onResume callback is NULL");
    }

    if (activity->callbacks->onInputQueueCreated) {
        l_info("[main] creating AInputQueue...");
        AInputQueue *aInputQueue = AInputQueue_create();
        activity->callbacks->onInputQueueCreated(activity, aInputQueue);
        l_success("[main] onInputQueueCreated OK");
    } else {
        l_warn("[main] onInputQueueCreated is NULL");
    }

    if (activity->callbacks->onNativeWindowCreated) {
        l_info("[main] creating ANativeWindow...");
        ANativeWindow *aNativeWindow = ANativeWindow_create();
        activity->callbacks->onNativeWindowCreated(activity, aNativeWindow);
        l_success("[main] onNativeWindowCreated OK");
    } else {
        l_warn("[main] onNativeWindowCreated is NULL");
    }

    if (activity->callbacks->onWindowFocusChanged) {
        l_info("[main] onWindowFocusChanged(1)...");
        activity->callbacks->onWindowFocusChanged(activity, 1);
        l_success("[main] onWindowFocusChanged OK");
    } else {
        l_warn("[main] onWindowFocusChanged is NULL");
    }

    l_info("[main] NDK lifecycle done — entering idle loop");
    unsigned frame = 0;
    while (1) {
        if ((frame % 120) == 0)
            l_info("[main] NDK idle alive, frame=%u", frame);
        gl_swap();
        frame++;
        sceKernelDelayThread(8000);
    }
#endif

    l_info("[main] exiting process");
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
