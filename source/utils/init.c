/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021-2022 Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/init.h"

#include "utils/dialog.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "utils/settings.h"

#include <reimpl/controls.h>

#include <string.h>
#include <stdlib.h>

#include <psp2/appmgr.h>
#include <psp2/apputil.h>
#include <psp2/kernel/clib.h>
#include <psp2/power.h>
#include <psp2/io/fcntl.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>
#include <fios/fios.h>

// Same address used by mc3-vita / gdash-vita / most so_loader ports
#define LOAD_ADDRESS 0x98000000

extern so_module so_mod;

/*
 * Write a tiny breadcrumb file so we know the last init_array index even if
 * the process crashes before log.txt is fully flushed.
 */
static void write_last_init_breadcrumb(uint32_t index, uint32_t total, uintptr_t fn) {
    char buf[256];
    int n = sceClibSnprintf(buf, sizeof(buf),
                            "index=%u/%u fn=0x%08X\n",
                            (unsigned)index, (unsigned)total, (unsigned)fn);
    if (n > 0)
        file_save(DATA_PATH "last_init.txt", (const uint8_t *)buf, (size_t)n);
}

/*
 * Same as so_util's so_initialize(), but logs every entry so a crash pinpoints
 * which .init_array constructor is fatal.
 */
static void so_initialize_logged(so_module *mod) {
    uint32_t total = mod->num_init_array;

    l_info("init_array count = %u", (unsigned)total);
    if (total == 0) {
        l_warn("No init_array entries — nothing to run.");
        return;
    }

    if (!mod->init_array) {
        l_fatal("init_array pointer is NULL but num_init_array=%u", (unsigned)total);
        return;
    }

    for (uint32_t i = 0; i < total; i++) {
        void (*fn)(void) = mod->init_array[i];

        if (!fn || fn == (void (*)(void))-1) {
            l_info("init_array[%u/%u] SKIP (null or -1)", (unsigned)i, (unsigned)total);
            continue;
        }

        uintptr_t addr = (uintptr_t)fn;
        l_info("init_array[%u/%u] CALL fn=0x%08X (offset from LOAD=0x%08X)",
               (unsigned)i, (unsigned)total,
               (unsigned)addr,
               (unsigned)(addr - (uintptr_t)LOAD_ADDRESS));

        /* Breadcrumb BEFORE the call — survives hard crash */
        write_last_init_breadcrumb(i, total, addr);

        fn();

        l_success("init_array[%u/%u] RETURNED OK", (unsigned)i, (unsigned)total);
        write_last_init_breadcrumb(i, total, addr); /* mark completed */
    }

    l_success("All %u init_array entries finished.", (unsigned)total);
}

void soloader_init_all() {
    l_info("=== Zombie Shooter Vita Port - soloader_init_all() start ===");
    l_info("DATA_PATH=%s", DATA_PATH);
    l_info("SO_PATH=%s", SO_PATH);
    l_info("LOAD_ADDRESS=0x%08X", (unsigned)LOAD_ADDRESS);

    // Launch `app0:configurator.bin` on `-config` init param
    sceAppUtilInit(&(SceAppUtilInitParam){}, &(SceAppUtilBootParam){});
    SceAppUtilAppEventParam eventParam;
    sceClibMemset(&eventParam, 0, sizeof(SceAppUtilAppEventParam));
    sceAppUtilReceiveAppEvent(&eventParam);
    if (eventParam.type == 0x05) {
        char buffer[2048];
        sceAppUtilAppEventParseLiveArea(&eventParam, buffer);
        if (strstr(buffer, "-config"))
            sceAppMgrLoadExec("app0:/configurator.bin", NULL, NULL);
    }

    // Set default overclock values
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);
    l_info("Clocks set: ARM 444 / BUS 222 / GPU 222 / XBAR 166");

#ifdef USE_SCELIBC_IO
    l_info("Initializing FIOS with path %s ...", DATA_PATH);
    if (fios_init(DATA_PATH) == 0)
        l_success("FIOS initialized.");
    else
        l_warn("FIOS init failed (continuing anyway).");
#else
    l_info("USE_SCELIBC_IO is OFF - skipping FIOS.");
#endif

    // --- kubridge presence ---
    l_info("Checking if kubridge module is loaded...");
    if (!module_loaded("kubridge")) {
        l_fatal("kubridge is NOT loaded.");
        fatal_error("You need to install kubridge.skprx to play this game.\n"
                    "Download: https://github.com/bythos14/kubridge/releases\n"
                    "Put it under *KERNEL in taiHEN config.txt and reboot.");
    }
    l_success("kubridge module is loaded.");

    // --- kubridge version (same hashes as mc3-vita) ---
    l_info("Checking kubridge.skprx version (SHA1)...");
    char *kubridge_hash = file_sha1sum("ux0:/tai/kubridge.skprx");
    if (!kubridge_hash)
        kubridge_hash = file_sha1sum("ur0:/tai/kubridge.skprx");

    if (!kubridge_hash) {
        l_fatal("kubridge.skprx file not found on disk (ux0:/tai or ur0:/tai).");
        fatal_error("Could not find kubridge.skprx file despite the plugin "
                    "itself being active. Please put it in either ur0:/tai or "
                    "ux0:/tai folder.");
    }

    l_info("kubridge SHA1: %s", kubridge_hash);

    const char *ver_01 = "v0.1 (TheFloW)";
    const char *ver_02 = "v0.2 (Bythos)";
    const char *ver_03 = "v0.3 (Bythos)";
    char *currently_installed_version = NULL;

    if (strcmp(kubridge_hash, "E033D76A90C9B8F2D496735C2692AFD8C3ED32FE") == 0)
        currently_installed_version = (char *)ver_01;
    else if (strcmp(kubridge_hash, "6CFC985904F9BBE3A4F54DD96197F5DF3E523DCB") == 0)
        currently_installed_version = (char *)ver_02;
    else if (strcmp(kubridge_hash, "AFAC6077618245D87CFF9ED2819223E6BB2DE5F8") == 0)
        currently_installed_version = (char *)ver_03;

    if (currently_installed_version) {
        l_fatal("kubridge version too old: %s", currently_installed_version);
        free(kubridge_hash);
        fatal_error("You need to update kubridge.skprx to version v0.3.1 or higher.\n"
                    "Currently installed: %s\n"
                    "Download: https://github.com/bythos14/kubridge/releases",
                    currently_installed_version);
    }

    l_success("kubridge version check passed (not a known old build).");
    free(kubridge_hash);

    // --- SO file existence ---
    l_info("Checking SO file exists: %s", SO_PATH);
    if (!file_exists(SO_PATH)) {
        l_fatal("SO file MISSING at %s", SO_PATH);
        fatal_error("Looks like you haven't installed the data files for this "
                    "port, or they are in an incorrect location.\n"
                    "Required file:\n%s",
                    SO_PATH);
    }

    size_t so_sz = file_size(SO_PATH);
    l_success("SO file found. Size = %u bytes (0x%X)", (unsigned)so_sz, (unsigned)so_sz);

    // --- Load SO ---
    l_info("Calling so_file_load(path=%s, addr=0x%08X)...", SO_PATH, (unsigned)LOAD_ADDRESS);
    int load_res = so_file_load(&so_mod, SO_PATH, LOAD_ADDRESS);
    if (load_res < 0) {
        l_fatal("so_file_load FAILED. return=0x%08X (%d)", (unsigned)load_res, load_res);
        l_fatal("Common causes: old kubridge, missing fd_fix, memory map failure.");
        fatal_error("Error: could not load\n%s\n\n"
                    "Error code: 0x%08X (%d)\n\n"
                    "1) Install kubridge v0.3.1+ from bythos14\n"
                    "2) Install fd_fix.skprx under *KERNEL\n"
                    "3) Reboot the Vita\n"
                    "4) Send ux0:data/zombieshooter/log.txt",
                    SO_PATH, (unsigned)load_res, load_res);
    }
    l_success("SO loaded successfully at 0x%08X.", (unsigned)LOAD_ADDRESS);

    l_info("Loading settings...");
    settings_load();
    l_success("Settings loaded.");

    l_info("Relocating SO...");
    so_relocate(&so_mod);
    l_success("SO relocated.");

    l_info("Resolving imports (dynlib)...");
    resolve_imports(&so_mod);
    l_success("SO imports resolved.");

    l_info("Applying patches...");
    so_patch();
    l_success("SO patched.");

    l_info("Flushing caches...");
    so_flush_caches(&so_mod);
    l_success("SO caches flushed.");

    l_info("Running SO init arrays (logged per entry)...");
    so_initialize_logged(&so_mod);
    l_success("SO initialized.");

    l_info("OpenGL preload...");
    gl_preload();
    l_success("OpenGL preloaded.");

    l_info("FalsoJNI init...");
    jni_init();
    l_success("FalsoJNI initialized.");

#ifndef NDK_PORT
    l_info("Controls init...");
    controls_init();
    l_success("Controls initialized.");
#endif

    l_success("=== soloader_init_all() COMPLETE ===");
}
