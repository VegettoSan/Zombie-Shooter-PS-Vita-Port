/*
 * Copyright (C) 2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  patch.c
 * @brief Patching some of the .so internal functions or bridging them to native
 *        for better compatibility.
 */

#include <kubridge.h>
#include <so_util/so_util.h>
#include <stdio.h>
#include <string.h>
#include <vitasdk.h>

#ifdef __cplusplus
extern "C"
{
#endif
	extern so_module so_mod;
#ifdef __cplusplus
};
#endif

#define SCE_KERNEL_MEMBLOCK_TYPE_USER_RX (0x0C20D050)

#include "utils/logger.h"
#include "utils/dialog.h"
#include "utils/so_trace.h"
#include "utils/perf.h"
#include "utils/engine_probe.h"
#include "reimpl/sys.h"
#include <stdbool.h>

#define KUSER_BLOCK_BASE 0x9A000000u
#define KUSER_MEMORY_BARRIER_ANDROID 0xFFFF0FA0u
#define KUSER_CMPXCHG_ANDROID 0xFFFF0FC0u
#define KUSER_MEMORY_BARRIER_VITA (KUSER_BLOCK_BASE + 0xFA0u)
#define KUSER_CMPXCHG_VITA (KUSER_BLOCK_BASE + 0xFC0u)

#ifdef NDK_PORT
/*
 * The game performs its real startup from APP_CMD_START.  Keep these hooks
 * diagnostic-only: they preserve the original result and let hardware logs
 * distinguish an event-loop exit from a failed Application::initialize().
 */
static so_hook app_run_hook;
static so_hook event_loop_run_hook;
static so_hook process_activity_hook;
static so_hook app_initialize_hook;
static so_hook core_initialize_hook;
static so_hook app_is_running_hook;
static so_hook force_finish_hook;
static so_hook on_start_application_hook;
static so_hook native_exit_hook;
static so_hook analytics_initialize_hook;
static so_hook ad_initialize_hook;
static so_hook configuration_initialize_hook;
static so_hook locale_initialize_hook;
static so_hook purchases_initialize_hook;
static so_hook score_initialize_hook;
static so_hook social_initialize_hook;
static so_hook sound_initialize_hook;
static so_hook text_initialize_hook;
static so_hook registry_load_value_hook;

static uintptr_t hooked_registry_load_value(void *result, void *self,
                                            const void *key, const void *fallback) {
    /* The exact SO constructor publishes Registry in its singleton at
     * +0x003e9394, but stores Registry::backend at self+12 only at
     * +0x003e93a8. A second thread called loadValue in that interval
     * (log_0004 core: PC=so+0x003eb11e, backend=NULL), while the
     * constructor was still on the other thread stack at so+0x003e93a3.
     * Wait only for this observed publication race. */
    volatile uintptr_t *backend = (volatile uintptr_t *)((uint8_t *)self + 12);
    if (*backend == 0) {
        l_warn("[REGISTRY] loadValue before constructor complete self=%p caller=%p",
               self, __builtin_return_address(0));
        unsigned waited_ms = 0;
        while (*backend == 0 && waited_ms < 5000) {
            sceKernelDelayThread(1000);
            waited_ms++;
        }
        l_info("[REGISTRY] loadValue waited=%u ms backend=%p",
               waited_ms, (void *)*backend);
        if (*backend == 0)
            l_error("[REGISTRY] constructor timeout; original loadValue will run for diagnosis");
    }
    return SO_CONTINUE(uintptr_t, registry_load_value_hook, result, self,
                       key, fallback);
}

static void log_hook_caller(const char *name, uintptr_t caller) {
	uintptr_t offset = 0;
	if (so_trace_offset(caller, &offset))
		l_info("[startup] %s caller=%p (so+0x%08X)", name,
			(void *)caller, (unsigned)offset);
	else
		l_info("[startup] %s caller=%p", name, (void *)caller);
}

static int hooked_app_run(void *self) {
	l_info("[startup] ApplicationNative::run ENTER self=%p", self);
	int result = SO_CONTINUE(int, app_run_hook, self);
	l_warn("[startup] ApplicationNative::run EXIT result=%d", result);
	return result;
}

static int hooked_event_loop_run(void *self) {
	l_info("[startup] EventLoop::run ENTER self=%p", self);
	int result = SO_CONTINUE(int, event_loop_run_hook, self);
	l_warn("[startup] EventLoop::run EXIT result=%d", result);
	return result;
}

static int hooked_process_activity(void *self, int command) {
	l_info("[startup] EventLoop::processActivity command=%d ENTER", command);
	int result = SO_CONTINUE(int, process_activity_hook, self, command);
	l_info("[startup] EventLoop::processActivity command=%d EXIT", command);
	return result;
}

static int hooked_app_initialize(void *self) {
	l_info("[startup] ApplicationNative::initialize ENTER self=%p", self);
	int result = SO_CONTINUE(int, app_initialize_hook, self);
	l_info("[startup] ApplicationNative::initialize EXIT result=%d", result);
	return result;
}

typedef struct startup_component_node {
	struct startup_component_node *previous;
	struct startup_component_node *next;
	void *component;
} startup_component_node;

static void log_component_list(void *self) {
	uint8_t *application = (uint8_t *)self;
	startup_component_node *sentinel =
		(startup_component_node *)(application + 0x3d68);
	startup_component_node *node =
		*(startup_component_node **)(application + 0x3d6c);

	for (unsigned i = 0; node && node != sentinel && i < 64; ++i) {
		void *component = node->component;
		uintptr_t initialize = 0;
		uintptr_t offset = 0;
		if (component) {
			uintptr_t vtable = *(uintptr_t *)component;
			/* core::Application::initialize at so+0x3d6658 loads
			 * component->vtable[7] before its BLX (vtable+28). */
			if (vtable)
				initialize = ((uintptr_t *)vtable)[7];
		}

		if (initialize && so_trace_offset(initialize, &offset))
			l_info("[startup] component[%u] self=%p initialize=%p (so+0x%08X)",
				i, component, (void *)initialize, (unsigned)offset);
		else
			l_info("[startup] component[%u] self=%p initialize=%p",
				i, component, (void *)initialize);

		node = node->next;
	}
}

static int hooked_core_initialize(void *self) {
	l_info("[startup] core::Application::initialize ENTER self=%p", self);
	log_component_list(self);
	int result = SO_CONTINUE(int, core_initialize_hook, self);
	l_info("[startup] core::Application::initialize EXIT result=%d", result);
	return result;
}

#define DEFINE_COMPONENT_INITIALIZE_HOOK(tag, hook_var, display_name) \
	static int hooked_##tag##_initialize(void *self, const void *delegate) { \
		l_info("[startup] component " display_name " initialize ENTER self=%p", self); \
		int result = SO_CONTINUE(int, hook_var, self, delegate); \
		l_info("[startup] component " display_name " initialize EXIT result=%d", \
			result != 0); \
		return result; \
	}

DEFINE_COMPONENT_INITIALIZE_HOOK(analytics, analytics_initialize_hook,
	"analytics::Analytics")
DEFINE_COMPONENT_INITIALIZE_HOOK(ad, ad_initialize_hook,
	"ad::AdManager")
DEFINE_COMPONENT_INITIALIZE_HOOK(configuration, configuration_initialize_hook,
	"core::Configuration")
DEFINE_COMPONENT_INITIALIZE_HOOK(locale, locale_initialize_hook,
	"core::Locale")
DEFINE_COMPONENT_INITIALIZE_HOOK(purchases, purchases_initialize_hook,
	"iap::Purchases")
DEFINE_COMPONENT_INITIALIZE_HOOK(score, score_initialize_hook,
	"ScoreSystem")
DEFINE_COMPONENT_INITIALIZE_HOOK(social, social_initialize_hook,
	"social::SocialEngine")
DEFINE_COMPONENT_INITIALIZE_HOOK(sound, sound_initialize_hook,
	"sound::Engine")
DEFINE_COMPONENT_INITIALIZE_HOOK(text, text_initialize_hook,
	"text_engine::TextEngine")

static int hooked_app_is_running(void *self) {
	static int previous = -1;
	int result = SO_CONTINUE(int, app_is_running_hook, self);
	result = result != 0;
	if (result != previous) {
		previous = result;
		l_info("[startup] ApplicationNative::isRunning changed -> %d", result);
		log_hook_caller("ApplicationNative::isRunning",
			(uintptr_t)__builtin_return_address(0));
	}
	return result;
}

static int hooked_force_finish(void *self) {
	l_warn("[startup] ApplicationNative::forceFinish ENTER self=%p", self);
	log_hook_caller("ApplicationNative::forceFinish",
		(uintptr_t)__builtin_return_address(0));
	int result = SO_CONTINUE(int, force_finish_hook, self);
	l_warn("[startup] ApplicationNative::forceFinish EXIT");
	return result;
}

static int hooked_on_start_application(void *self) {
	l_info("[startup] ApplicationNative::onStartApplication ENTER self=%p", self);
	int result = SO_CONTINUE(int, on_start_application_hook, self);
	l_info("[startup] ApplicationNative::onStartApplication EXIT result=%d",
		result != 0);
	return result;
}

static int hooked_native_exit(void *self) {
	l_warn("[startup] ApplicationNative::nativeExit ENTER self=%p", self);
	log_hook_caller("ApplicationNative::nativeExit",
		(uintptr_t)__builtin_return_address(0));
	int result = SO_CONTINUE(int, native_exit_hook, self);
	l_warn("[startup] ApplicationNative::nativeExit EXIT");
	return result;
}

static so_hook install_startup_hook(const char *symbol, uintptr_t replacement) {
	uintptr_t address = so_symbol(&so_mod, symbol);
	if (!address) {
		l_warn("[PATCH] startup hook symbol missing: %s", symbol);
		so_hook empty = {0};
		return empty;
	}
	l_info("[PATCH] startup hook %s @ %p", symbol, (void *)address);
	return hook_addr(address, replacement);
}

static void install_startup_diagnostics(void) {
	app_run_hook = install_startup_hook(
		"_ZN7android17ApplicationNative3runEv", (uintptr_t)&hooked_app_run);
	event_loop_run_hook = install_startup_hook(
		"_ZN7android9EventLoop3runEv", (uintptr_t)&hooked_event_loop_run);
	process_activity_hook = install_startup_hook(
		"_ZN7android9EventLoop15processActivityEi", (uintptr_t)&hooked_process_activity);
	app_initialize_hook = install_startup_hook(
		"_ZN7android17ApplicationNative10initializeEv", (uintptr_t)&hooked_app_initialize);
	core_initialize_hook = install_startup_hook(
		"_ZN4core11Application10initializeEv", (uintptr_t)&hooked_core_initialize);
	app_is_running_hook = install_startup_hook(
		"_ZN7android17ApplicationNative9isRunningEv", (uintptr_t)&hooked_app_is_running);
	force_finish_hook = install_startup_hook(
		"_ZN7android17ApplicationNative11forceFinishEv", (uintptr_t)&hooked_force_finish);
	on_start_application_hook = install_startup_hook(
		"_ZN7android17ApplicationNative18onStartApplicationEv",
		(uintptr_t)&hooked_on_start_application);
	native_exit_hook = install_startup_hook(
		"_ZN7android17ApplicationNative10nativeExitEv",
		(uintptr_t)&hooked_native_exit);

	/*
	 * Component initialization is virtual. Most entries below are the actual
	 * non-virtual thunks stored in the AbstractComponent vtables. Configuration
	 * and sound::Engine use tiny branch thunks, while Locale's thunk begins on a
	 * halfword boundary; hook their primary exported functions instead. Registry
	 * is omitted because its 16-byte implementation unconditionally returns true.
	 */
	analytics_initialize_hook = install_startup_hook(
		"_ZThn4_N9analytics9Analytics10initializeERKN4core14EngineDelegateE",
		(uintptr_t)&hooked_analytics_initialize);
	ad_initialize_hook = install_startup_hook(
		"_ZThn4_N2ad9AdManager10initializeERKN4core14EngineDelegateE",
		(uintptr_t)&hooked_ad_initialize);
	configuration_initialize_hook = install_startup_hook(
		"_ZN4core13Configuration10initializeERKNS_14EngineDelegateE",
		(uintptr_t)&hooked_configuration_initialize);
	locale_initialize_hook = install_startup_hook(
		"_ZN4core6Locale10initializeERKNS_14EngineDelegateE",
		(uintptr_t)&hooked_locale_initialize);
	purchases_initialize_hook = install_startup_hook(
		"_ZThn4_N3iap9Purchases10initializeERKN4core14EngineDelegateE",
		(uintptr_t)&hooked_purchases_initialize);
	score_initialize_hook = install_startup_hook(
		"_ZThn4_N11ScoreSystem10initializeERKN4core14EngineDelegateE",
		(uintptr_t)&hooked_score_initialize);
	social_initialize_hook = install_startup_hook(
		"_ZN6social12SocialEngine10initializeERKN4core14EngineDelegateE",
		(uintptr_t)&hooked_social_initialize);
	sound_initialize_hook = install_startup_hook(
		"_ZN5sound6Engine10initializeERKN4core14EngineDelegateE",
		(uintptr_t)&hooked_sound_initialize);
	text_initialize_hook = install_startup_hook(
		"_ZThn4_N11text_engine10TextEngine10initializeERKN4core14EngineDelegateE",
		(uintptr_t)&hooked_text_initialize);
	registry_load_value_hook = install_startup_hook(
		"_ZN4core8Registry9loadValueERK6STRINGS3_",
		(uintptr_t)&hooked_registry_load_value);
}
#endif

void __kuser_memory_barrier(void) {
	__sync_synchronize();
}

/* Low-frequency engine phases, no instruction writes/cache flush per call.
 * These five canonical prologues end exactly at byte 8 and do not use PC.
 * Opaque r0 return preserved; all explicit arguments are this + optional int.
 * No floating-point, hidden result, or stack arguments in these signatures. */
#ifdef NDK_PORT
static uintptr_t engine_original[PERF_ENGINE_COUNT];
#define ENGINE_INT_PROBE(tag, phase) \
static uintptr_t probe_##tag(void *self,int argument) { \
    uint64_t start=sceKernelGetProcessTimeWide(); \
    uintptr_t ret=((uintptr_t (*)(void *,int))engine_original[phase])(self,argument); \
    perf_engine_phase(phase,start);return ret; \
}
#define ENGINE_THIS_PROBE(tag, phase) \
static uintptr_t probe_##tag(void *self) { \
    uint64_t start=sceKernelGetProcessTimeWide(); \
    uintptr_t ret=((uintptr_t (*)(void *))engine_original[phase])(self); \
    perf_engine_phase(phase,start);return ret; \
}
ENGINE_INT_PROBE(graph,PERF_ENGINE_GRAPH)
ENGINE_INT_PROBE(software,PERF_ENGINE_SOFTWARE)
ENGINE_THIS_PROBE(map,PERF_ENGINE_MAP)
ENGINE_THIS_PROBE(pre,PERF_ENGINE_PRE)
ENGINE_INT_PROBE(post,PERF_ENGINE_POST)
/* DrawLayer has two by-reference VECTOR2s (pointer arguments), two bools on
 * incoming stack; no float ABI. Timed once per layer, not once per pixel. */
static uintptr_t probe_collector(void *self,int layer,const void *a,const void *b,bool c,bool d) {
    uint64_t start=sceKernelGetProcessTimeWide();
    uintptr_t ret=((uintptr_t (*)(void *,int,const void *,const void *,bool,bool))engine_original[PERF_ENGINE_COLLECTOR])(self,layer,a,b,c,d);
    perf_engine_phase(PERF_ENGINE_COLLECTOR,start);return ret;
}
void raster_palette_install(void);
void raster_alpha_install(void);
static void install_engine_probes(void) {
    static const struct {
        const char *symbol;unsigned offset;uint32_t prologue[2];uintptr_t replacement;
    } probes[]={
        {"_ZN5GRAPH4TactEi",0x410a30,{0xaf03b5f0,0x0f00e92d},(uintptr_t)probe_graph},
        {"_ZN5GRAPH12softwareTactEi",0x410458,{0xaf03b5f0,0x0f00e92d},(uintptr_t)probe_software},
        {"_ZN3MAP4tactEv",0x4372c4,{0xaf03b5f0,0x0f00e92d},(uintptr_t)probe_map},
        {"_ZN8OpenGLES7preTactEv",0x46aa2c,{0xaf03b5f0,0xbd04f84d},(uintptr_t)probe_pre},
        {"_ZN8OpenGLES8PostTactEi",0x46af1c,{0xaf03b5f0,0x8d04f84d},(uintptr_t)probe_post},
        {"_ZNK16SPRITE_COLLECTOR9DrawLayerEiRK7VECTOR2S2_bb",0x4f4b20,{0xaf03b5f0,0x0f00e92d},(uintptr_t)probe_collector}
    };
    for(unsigned i=0;i<PERF_ENGINE_COUNT;++i) {
        uintptr_t address=(uintptr_t)so_symbol(&so_mod,probes[i].symbol);
        uintptr_t entry=address&~(uintptr_t)1;
        uintptr_t arena=(so_mod.patch_head+3)&~(uintptr_t)3;
        if(!(address&1) || entry!=so_mod.load_addr+probes[i].offset ||
           arena<so_mod.patch_base || arena>so_mod.patch_base+so_mod.patch_size ||
           so_mod.patch_base+so_mod.patch_size-arena<16) {
            l_warn("[PATCH] engine probe disabled: %s address/arena mismatch",probes[i].symbol);continue;
        }
        uint32_t code[4];
        if(!engine_probe_trampoline(code,(void *)entry,probes[i].prologue,(uint32_t)(entry+8))) {
            l_warn("[PATCH] engine probe disabled: %s prologue mismatch",probes[i].symbol);continue;
        }
        sceClibMemcpy((void *)arena,code,sizeof(code));
        engine_original[i]=arena|1;
        so_mod.patch_head=arena+sizeof(code);
        hook_addr(address,probes[i].replacement);
        kuKernelFlushCaches((void *)arena,sizeof(code));
        kuKernelFlushCaches((void *)entry,8);
        l_info("[PATCH] engine probe installed: %s so+0x%X",probes[i].symbol,probes[i].offset);
    }
}
#endif

static void patch_kuser_pointer(const char *symbol, uint32_t android_addr,
		uint32_t vita_addr) {
	uintptr_t symbol_addr = so_symbol(&so_mod, symbol);
	if (!symbol_addr) {
		l_warn("[PATCH] kuser pointer symbol not found: %s", symbol);
		return;
	}

	uint32_t current = *(volatile uint32_t *)symbol_addr;
	l_info("[PATCH] %s @ 0x%08X: 0x%08X -> 0x%08X", symbol,
		(unsigned)symbol_addr, (unsigned)current, (unsigned)vita_addr);

	if (current != android_addr && current != vita_addr) {
		l_warn("[PATCH] unexpected original kuser pointer 0x%08X", (unsigned)current);
	}

	kuKernelCpuUnrestrictedMemcpy((void *)symbol_addr, &vita_addr,
		sizeof(vita_addr));
}

void kuser_patch(void) {
	SceKernelAllocMemBlockKernelOpt opt;
	memset(&opt, 0, sizeof(SceKernelAllocMemBlockKernelOpt));
	opt.size = sizeof(SceKernelAllocMemBlockKernelOpt);
	opt.attr = 0x1;
	opt.field_C = (SceUInt32)KUSER_BLOCK_BASE;
	if (kuKernelAllocMemBlock("atomic", SCE_KERNEL_MEMBLOCK_TYPE_USER_RX, 0x1000, &opt) < 0)
		fatal_error("Error could not allocate atomic block.");
	kuKernelMemProtect((void *)KUSER_BLOCK_BASE, (SceSize)0x1000,
		KU_KERNEL_PROT_EXEC | KU_KERNEL_PROT_READ | KU_KERNEL_PROT_WRITE);

	hook_addr(KUSER_MEMORY_BARRIER_VITA, (uintptr_t)__kuser_memory_barrier);
	hook_addr(KUSER_CMPXCHG_VITA, (uintptr_t)__atomic_cmpxchg);
	kuKernelFlushCaches((void *)KUSER_BLOCK_BASE, 0x1000);

	uint32_t patched_addr;
	uint32_t text_patches = 0;
	for (uint32_t addr = so_mod.text_base; addr < so_mod.text_base + so_mod.text_size; addr += 4) {
		uint32_t *a = (uint32_t *)addr;
		if (*a == KUSER_CMPXCHG_ANDROID) {
			l_debug("Patching 0x%x -> __kuser_cmpxchg", a);
			patched_addr = KUSER_CMPXCHG_VITA;
			kuKernelCpuUnrestrictedMemcpy((void *)(addr), &patched_addr, sizeof(uint32_t));
			text_patches++;
		}
		else if (*a == KUSER_MEMORY_BARRIER_ANDROID) {
			l_debug("Patching 0x%x -> __kuser_memory_barrier", a);
			patched_addr = KUSER_MEMORY_BARRIER_VITA;
			kuKernelCpuUnrestrictedMemcpy((void *)(addr), &patched_addr, sizeof(uint32_t));
			text_patches++;
		}
	}
	l_info("[PATCH] direct kuser constants patched in .text: %u",
		(unsigned)text_patches);

	/*
	 * This protobuf build stores the Android kuser addresses in exported
	 * function-pointer objects in .data. Scanning only .text leaves those
	 * pointers at 0xFFFF0FA0/0xFFFF0FC0 and the generated descriptor
	 * constructors jump to unmapped Android kernel space.
	 */
	patch_kuser_pointer(
		"_ZN6google8protobuf8internal25pLinuxKernelMemoryBarrierE",
		KUSER_MEMORY_BARRIER_ANDROID, KUSER_MEMORY_BARRIER_VITA);
	patch_kuser_pointer(
		"_ZN6google8protobuf8internal19pLinuxKernelCmpxchgE",
		KUSER_CMPXCHG_ANDROID, KUSER_CMPXCHG_VITA);
}

void so_patch(void) {
	kuser_patch();
#ifdef NDK_PORT
	install_startup_diagnostics();
	install_engine_probes();
	raster_palette_install();
	raster_alpha_install();
#endif
	// Sample hook with symbol name
	// hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN6glitch2os7Printer5printEPKcz"), (uintptr_t)&hookedFunction);
	// Or with offset
	// hook_addr((uintptr_t)so_mod.text_base + 0xdeadbabe, (uintptr_t)&hookedFunction);
	// If you use SO_CONTINUE, define a so_hook before the function and assign to it
	// function_hook = hook_addr(...);
}
