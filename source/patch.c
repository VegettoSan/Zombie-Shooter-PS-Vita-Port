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
#include "utils/settings.h"
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


#include <stdlib.h>
/* ZS361_DIRECT_REGISTRY_SAVE
 *
 * Build 1161's Registry::setString/getString route persistent encrypted values
 * through storeEncrypted/loadDecrypted.  OpenSSL AES-256-CBC is unavailable in
 * the Vita runtime provider, so bypass only that crypto boundary and persist
 * the original STRING value through the direct Vita Registry backend.
 * Registry::encryptKey remains the game's own implementation, preserving the
 * exact key namespace expected by contains/remove/enumeration.
 */
extern int zombie_registry_direct_put(const char *key, const char *value);
extern char *zombie_registry_direct_get(const char *key);

static so_hook zs361_registry_store_encrypted_hook;
static so_hook zs361_registry_load_decrypted_hook;

typedef const char *(*zs361_string_c_str_fn)(const void *self);
typedef void (*zs361_string_ctor_cstr_fn)(void *self, const char *value);
typedef void (*zs361_string_copy_ctor_fn)(void *self, const void *other);
typedef void (*zs361_string_dtor_fn)(void *self);
typedef void (*zs361_encrypt_key_fn)(void *result, void *registry, const void *key);

static zs361_string_c_str_fn zs361_string_c_str;
static zs361_string_ctor_cstr_fn zs361_string_ctor_cstr;
static zs361_string_copy_ctor_fn zs361_string_copy_ctor;
static zs361_string_dtor_fn zs361_string_dtor;
static zs361_encrypt_key_fn zs361_encrypt_key;

typedef struct {
    uintptr_t words[3]; /* build-1161 STRING is exactly 12 bytes */
} zs361_string_storage;

#define ZS361_REG_STORE_ENCRYPTED_ENTRY 0x003FB984u
#define ZS361_REG_LOAD_DECRYPTED_ENTRY  0x003FB054u
#define ZS361_REG_ENCRYPT_KEY_ENTRY     0x003FBFF4u
#define ZS361_STRING_C_STR_ENTRY        0x003ED768u
#define ZS361_STRING_CTOR_CSTR_ENTRY    0x003ED7B0u
#define ZS361_STRING_COPY_CTOR_ENTRY    0x003ED800u
#define ZS361_STRING_DTOR_ENTRY         0x003EE644u

static uintptr_t zs361_even(uintptr_t address) {
    return address & ~(uintptr_t)1u;
}

static uintptr_t zs361_require_thumb_symbol(const char *name, uintptr_t expected_offset) {
    uintptr_t symbol = so_symbol(&so_mod, name);
    uintptr_t expected = so_mod.load_addr + expected_offset;
    if (!symbol || !(symbol & 1u) || zs361_even(symbol) != expected) {
        l_error("[SAVE] refusing direct Registry hook %s: symbol=%p expected=so+0x%08X Thumb",
                name, (void *)symbol, (unsigned)expected_offset);
        return 0;
    }
    return symbol;
}

static int zs361_validate_prologue(uintptr_t symbol, const uint8_t expected[8],
                                   const char *name) {
    uintptr_t entry = zs361_even(symbol);
    if (memcmp((const void *)entry, expected, 8) != 0) {
        l_error("[SAVE] refusing direct Registry hook %s: build-1161 prologue mismatch", name);
        return 0;
    }
    return 1;
}

static int zs361_make_backend_key(zs361_string_storage *out, void *registry,
                                  const void *key, const char **chars_out) {
    memset(out, 0, sizeof(*out));
    zs361_encrypt_key(out, registry, key);
    const char *chars = zs361_string_c_str(out);
    if (!chars || !*chars) {
        zs361_string_dtor(out);
        return 0;
    }
    *chars_out = chars;
    return 1;
}

static void zs361_hooked_registry_store_encrypted(void *registry,
                                                   const void *key,
                                                   const void *value) {
    zs361_string_storage backend_key;
    const char *backend_chars = NULL;
    const char *value_chars = zs361_string_c_str(value);
    if (!value_chars || !zs361_make_backend_key(&backend_key, registry, key, &backend_chars)) {
        l_error("[SAVE] direct Registry store rejected invalid STRING/key");
        return;
    }

    int ok = zombie_registry_direct_put(backend_chars, value_chars);
#ifdef ZOMBIE_DEBUG_BUILD
    static unsigned reports;
    if (__atomic_fetch_add(&reports, 1, __ATOMIC_RELAXED) < 128)
        l_perf("[SAVE] Registry.storeEncrypted -> direct file bytes=%u ok=%d",
               (unsigned)strlen(value_chars), ok);
#endif
    zs361_string_dtor(&backend_key);
}

static void zs361_hooked_registry_load_decrypted(void *result,
                                                  void *registry,
                                                  const void *key,
                                                  const void *fallback) {
    zs361_string_storage backend_key;
    const char *backend_chars = NULL;
    char *value = NULL;
    int key_ok = zs361_make_backend_key(&backend_key, registry, key, &backend_chars);
    if (key_ok)
        value = zombie_registry_direct_get(backend_chars);

    if (value)
        zs361_string_ctor_cstr(result, value);
    else
        zs361_string_copy_ctor(result, fallback);

#ifdef ZOMBIE_DEBUG_BUILD
    static unsigned reports;
    if (__atomic_fetch_add(&reports, 1, __ATOMIC_RELAXED) < 128)
        l_perf("[SAVE] Registry.loadDecrypted <- direct file hit=%d bytes=%u",
               value != NULL, value ? (unsigned)strlen(value) : 0u);
#endif
    free(value);
    if (key_ok)
        zs361_string_dtor(&backend_key);
}

static void install_zs361_direct_registry_save(void) {
    static const uint8_t store_prologue[8] = {0xF0,0xB5,0x03,0xAF,0x2D,0xE9,0x00,0x0B};
    static const uint8_t load_prologue[8]  = {0xF0,0xB5,0x03,0xAF,0x4D,0xF8,0x04,0x8D};

    uintptr_t store = zs361_require_thumb_symbol(
        "_ZN4core8Registry14storeEncryptedERK6STRINGS3_", ZS361_REG_STORE_ENCRYPTED_ENTRY);
    uintptr_t load = zs361_require_thumb_symbol(
        "_ZN4core8Registry13loadDecryptedERK6STRINGS3_", ZS361_REG_LOAD_DECRYPTED_ENTRY);
    uintptr_t encrypt_key = zs361_require_thumb_symbol(
        "_ZN4core8Registry10encryptKeyERK6STRING", ZS361_REG_ENCRYPT_KEY_ENTRY);
    uintptr_t c_str = zs361_require_thumb_symbol(
        "_ZNK6STRING5c_strEv", ZS361_STRING_C_STR_ENTRY);
    uintptr_t ctor_cstr = zs361_require_thumb_symbol(
        "_ZN6STRINGC1EPKc", ZS361_STRING_CTOR_CSTR_ENTRY);
    uintptr_t copy_ctor = zs361_require_thumb_symbol(
        "_ZN6STRINGC1ERKS_", ZS361_STRING_COPY_CTOR_ENTRY);
    uintptr_t dtor = zs361_require_thumb_symbol(
        "_ZN6STRINGD1Ev", ZS361_STRING_DTOR_ENTRY);

    if (!store || !load || !encrypt_key || !c_str || !ctor_cstr || !copy_ctor || !dtor ||
        !zs361_validate_prologue(store, store_prologue, "Registry::storeEncrypted") ||
        !zs361_validate_prologue(load, load_prologue, "Registry::loadDecrypted")) {
        l_error("[SAVE] direct Registry file backend disabled; exact build-1161 guards failed");
        return;
    }

    zs361_encrypt_key = (zs361_encrypt_key_fn)encrypt_key;
    zs361_string_c_str = (zs361_string_c_str_fn)c_str;
    zs361_string_ctor_cstr = (zs361_string_ctor_cstr_fn)ctor_cstr;
    zs361_string_copy_ctor = (zs361_string_copy_ctor_fn)copy_ctor;
    zs361_string_dtor = (zs361_string_dtor_fn)dtor;

    zs361_registry_store_encrypted_hook = hook_addr(
        store, (uintptr_t)&zs361_hooked_registry_store_encrypted);
    zs361_registry_load_decrypted_hook = hook_addr(
        load, (uintptr_t)&zs361_hooked_registry_load_decrypted);
    l_info("[SAVE] installed 3.6.1 direct Registry backend -> ux0:data/zombieshooter/save/");
}


static uintptr_t hooked_registry_load_value(void *result, void *self,
                                            const void *key, const void *fallback) {
#ifdef ZOMBIE_DEBUG_BUILD
    static unsigned reads;
    if(__atomic_fetch_add(&reads,1,__ATOMIC_RELAXED)<128) l_perf("[SAVE] engine=Registry.loadValue key=%p caller=%p",key,__builtin_return_address(0));
#endif
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

/* Pass 11: MetalSyntax-style render reuse.  GRAPH::Tact/MAP/input/audio keep
 * running every engine tick; only the expensive software raster pass is
 * reused every other tick only after sustained costly renders. Cheap startup
 * frames render normally. The opaque original return is preserved so no
 * caller contract changes.  glTexSubImage2D_soloader sees the per-tick flag
 * and skips only OpenGLES::UnLock's full software-framebuffer upload. */
#include "utils/render_reuse_policy.h"
volatile int zombie_render_reuse_active_this_tick = 0;
static uintptr_t render_reuse_last_software_result;
static RenderReusePolicy render_policy;
static unsigned map_ticks;
static unsigned render_reuse_rendered;
static unsigned render_reuse_reused;
static int render_reuse_has_frame;

static uintptr_t probe_software(void *self,int argument) {
    extern int render_upload_reuse_ready(void);
    /* A deleted/redefined framebuffer must be produced before it can be reused. */
    int mode=render_upload_reuse_ready()?setting_software_frameskip:0;
    int reuse = render_reuse_choose(&render_policy,mode,(uintptr_t)self,argument);
    if (reuse) {
        zombie_render_reuse_active_this_tick = 1;
        render_reuse_reused++;
        return render_reuse_last_software_result;
    }
    zombie_render_reuse_active_this_tick = 0;
    uint64_t start=sceKernelGetProcessTimeWide();
    uintptr_t ret=((uintptr_t (*)(void *,int))engine_original[PERF_ENGINE_SOFTWARE])(self,argument);
    render_reuse_complete(&render_policy,(unsigned)(sceKernelGetProcessTimeWide()-start));
    perf_engine_phase(PERF_ENGINE_SOFTWARE,start);
    render_reuse_last_software_result=ret;
    render_reuse_has_frame=1;
    render_reuse_rendered++;
    return ret;
}

void perf_rates_report(uint64_t elapsed_us,unsigned presents) {
    static unsigned old_new,old_reuse,old_ticks;
    unsigned rendered=render_reuse_rendered,reused=render_reuse_reused;
    unsigned n=rendered-old_new,r=reused-old_reuse,ticks=map_ticks-old_ticks;
    l_perf("rates ticks_x10=%u presents_x10=%u new_software_frames_x10=%u reused_frames_x10=%u ticks=%u presents=%u new_frames=%u reused_frames=%u elapsed_ms=%u",
        (unsigned)((uint64_t)ticks*10000000/elapsed_us),(unsigned)((uint64_t)presents*10000000/elapsed_us),
        (unsigned)((uint64_t)n*10000000/elapsed_us),(unsigned)((uint64_t)r*10000000/elapsed_us),ticks,presents,n,r,(unsigned)(elapsed_us/1000));
    old_new=rendered;old_reuse=reused;old_ticks=map_ticks;
}
void render_reuse_report(void) {
    static unsigned old_rendered,old_reused;
    unsigned rendered=render_reuse_rendered, reused=render_reuse_reused;
    l_perf("render_reuse config=%d rendered_ticks=%u reused_ticks=%u has_frame=%d current_reuse=%d strategy=adaptive_software_cost expensive=%d enter_us=8000 exit_us=4000 consecutive=4",
        setting_software_frameskip,rendered-old_rendered,reused-old_reused,
        render_reuse_has_frame,zombie_render_reuse_active_this_tick,render_policy.expensive);
    old_rendered=rendered;old_reused=reused;
}

static uintptr_t probe_map(void *self) {
    map_ticks++;uint64_t start=sceKernelGetProcessTimeWide();
    uintptr_t ret=((uintptr_t (*)(void *))engine_original[PERF_ENGINE_MAP])(self);
    perf_engine_phase(PERF_ENGINE_MAP,start);return ret;
}
ENGINE_THIS_PROBE(pre,PERF_ENGINE_PRE)
ENGINE_INT_PROBE(post,PERF_ENGINE_POST)
/* DrawLayer has two by-reference VECTOR2s (pointer arguments), two bools on
 * incoming stack; no float ABI. Timed once per layer, not once per pixel. */
void raster_light_layer_record(int layer,unsigned us);
static uintptr_t probe_collector(void *self,int layer,const void *a,const void *b,bool c,bool d) {
    uint64_t start=sceKernelGetProcessTimeWide();
    uintptr_t ret=((uintptr_t (*)(void *,int,const void *,const void *,bool,bool))engine_original[PERF_ENGINE_COLLECTOR])(self,layer,a,b,c,d);
    raster_light_layer_record(layer,(unsigned)(sceKernelGetProcessTimeWide()-start));
    perf_engine_phase(PERF_ENGINE_COLLECTOR,start);return ret;
}
void raster_palette_install(void);
void audio_stream_install(void);
void render_scale_install(void);
void raster_alpha_install(void);
void raster_light_install(void);
void light_pipeline_install(void);
static void install_engine_probes(void) {
    extern void map_profile_install(void);
    map_profile_install();
    extern void asset_cache_configure(size_t);
    asset_cache_configure((size_t)setting_asset_cache_mib*1024*1024);
    extern void asset_index_configure_negative(int);
    asset_index_configure_negative(setting_asset_negative_cache);
    static const struct {
        const char *symbol;unsigned offset;uint32_t prologue[2];uintptr_t replacement;
    } probes[]={
        {"_ZN5GRAPH4TactEi",0x421d70,{0xaf03b5f0,0x0f00e92d},(uintptr_t)probe_graph},
        {"_ZN5GRAPH12softwareTactEi",0x421798,{0xaf03b5f0,0x0f00e92d},(uintptr_t)probe_software},
        {"_ZN3MAP4tactEv",0x448b00,{0xaf03b5f0,0x0f00e92d},(uintptr_t)probe_map},
        {"_ZN8OpenGLES7preTactEv",0x47d9a0,{0xaf03b5f0,0xbd04f84d},(uintptr_t)probe_pre},
        {"_ZN8OpenGLES8PostTactEi",0x47de90,{0xaf03b5f0,0x8d04f84d},(uintptr_t)probe_post},
        {"_ZNK16SPRITE_COLLECTOR9DrawLayerEiRK7VECTOR2S2_bb",0x5108d0,{0xaf03b5f0,0x0f00e92d},(uintptr_t)probe_collector}
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

#include "contract_trace.inc"
void so_patch(void) {
	kuser_patch();
	install_zs361_direct_registry_save();
#ifdef NDK_PORT
	/* 3.6.1: startup diagnostics disabled until independently revalidated. */
	install_engine_probes();
    /* 3.6.1: legacy contract traces disabled. */
	/* 3.6.1: old palette hook disabled. */
    audio_stream_install();
    render_scale_install();
	/* 3.6.1: old alpha hook disabled. */
	/* 3.6.1: old raster-light hook disabled. */
	/* 3.6.1: old light-pipeline hook disabled. */
#endif
	// Sample hook with symbol name
	// hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN6glitch2os7Printer5printEPKcz"), (uintptr_t)&hookedFunction);
	// Or with offset
	// hook_addr((uintptr_t)so_mod.text_base + 0xdeadbabe, (uintptr_t)&hookedFunction);
	// If you use SO_CONTINUE, define a so_hook before the function and assign to it
	// function_hook = hook_addr(...);
}
