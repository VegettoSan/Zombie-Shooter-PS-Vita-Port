/* MIT. Runtime-selectable work resolution for run #16 recovery A/B/C.
 * software_width=0 skips the hook entirely and preserves the Android engine
 * policy.  960 and 864 install the verified hook and only cap the original
 * factor.  Physical DisplayMetrics remain 960x544 at 220 DPI. */
#include <stdint.h>
#include <string.h>
#include <so_util/so_util.h>
#include <kubridge.h>
#include <psp2/kernel/clib.h>
#include "utils/logger.h"
#include "utils/settings.h"
#include "utils/engine_probe.h"
#include "utils/render_scale_policy.h"

extern so_module so_mod;

static void (*original_calculate)(const void *);
static float (*get_factor)(void);
static void (*set_factor)(float);

static void calculate(const void *config) {
    original_calculate(config);

    uint32_t dimensions[2];
    memcpy(dimensions, config, sizeof(dimensions));

    float before = get_factor();
    float after = render_scale_cap(dimensions[0], dimensions[1], before,
                                   setting_software_width);
    if (after != before)
        set_factor(after);

    static unsigned reports;
    if (reports++ < 4) {
        l_perf("work_resolution requested_width=%d physical=%ux%u factor_before=%.6f factor_after=%.6f predicted=%ux%u",
               setting_software_width, dimensions[0], dimensions[1],
               (double)before, (double)after,
               (unsigned)(dimensions[0] * after),
               (unsigned)(dimensions[1] * after));
    }
}

void render_scale_install(void) {
    /* Important diagnostic distinction: width 0 means the potentially
     * problematic trampoline is not installed at all. */
    if (setting_software_width == 0) {
        l_perf("work_resolution installed=0 requested_width=0 mode=original hook_skipped=1 expected_surface=1024x580");
        return;
    }

    if (setting_software_width != 864 && setting_software_width != 960) {
        l_perf("work_resolution installed=0 requested_width=%d reason=invalid_config hook_skipped=1",
               setting_software_width);
        return;
    }

    uintptr_t addr = (uintptr_t)so_symbol(
        &so_mod, "_ZN12scale_config15calculateFactorERKNS_19ScreenConfigurationE");
    uintptr_t getter = (uintptr_t)so_symbol(&so_mod, "_ZN12scale_config6factorEv");
    uintptr_t setter = (uintptr_t)so_symbol(&so_mod, "_ZN12scale_config9setFactorEf");
    uintptr_t entry = addr & ~(uintptr_t)1;
    uintptr_t arena = (so_mod.patch_head + 3) & ~(uintptr_t)3;
    const uint32_t prologue[] = {0xaf03b5f0, 0xbd04f84d};
    uint32_t code[4];

    if (!(addr & 1) || entry != so_mod.load_addr + 0x481db0 ||
        getter != so_mod.load_addr + 0x4820d1 ||
        setter != so_mod.load_addr + 0x4820dd ||
        arena < so_mod.patch_base || arena > so_mod.patch_base + so_mod.patch_size ||
        so_mod.patch_base + so_mod.patch_size - arena < sizeof(code) ||
        !engine_probe_trampoline(code, (void *)entry, prologue,
                                 (uint32_t)(entry + 8))) {
        l_perf("work_resolution installed=0 requested_width=%d reason=verified_hook_unavailable",
               setting_software_width);
        return;
    }

    sceClibMemcpy((void *)arena, code, sizeof(code));
    original_calculate = (void *)(arena | 1);
    get_factor = (void *)getter;
    set_factor = (void *)setter;
    so_mod.patch_head = arena + sizeof(code);

    hook_addr(addr, (uintptr_t)calculate);
    kuKernelFlushCaches((void *)arena, sizeof(code));
    kuKernelFlushCaches((void *)entry, 8);

    l_perf("work_resolution installed=1 requested_width=%d physical_display=960x544 dpi=220",
           setting_software_width);
}
