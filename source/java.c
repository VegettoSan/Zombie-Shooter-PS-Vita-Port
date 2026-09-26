#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_Impl.h>
#include <falso_jni/FalsoJNI_Logger.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <falso_ndk/android/AInput.h>
#include "utils/logger.h"

/*
 * JNI Methods
*/

enum {
	METHOD_GET_PACKAGE_NAME = 100,
	METHOD_GET_VERSION = 101,
	METHOD_GET_VERSION_CODE = 102,
	METHOD_GET_CLASS_LOADER = 103,
	METHOD_LOAD_CLASS = 104,
	METHOD_QUIT = 105,
	METHOD_GET_CONTENT_RESOLVER = 106,
	METHOD_SECURE_GET_STRING = 107,
    METHOD_DISPLAY_METRICS_INIT = 300,
    METHOD_GET_WINDOW_MANAGER,
    METHOD_GET_DEFAULT_DISPLAY,
    METHOD_DISPLAY_GET_METRICS,
    METHOD_INPUT_DEVICE_GET_DEVICE = 200,
    METHOD_INPUT_DEVICE_GET_ID,
    METHOD_INPUT_DEVICE_GET_SOURCES,
    METHOD_INPUT_DEVICE_GET_DEVICE_IDS,
    METHOD_INPUT_HELPER_GET_SOURCES,
    METHOD_INPUT_HELPER_GET_RANGES,
};

static jobject getClassLoader(jmethodID id, va_list args) {
	(void) id;
	(void) args;
	// Stable reserved object used only as the receiver for loadClass().
	return (jobject)0x69696969;
}

static jobject loadClass(jmethodID id, va_list args) {
	(void) id;
	jstring class_name = va_arg(args, jstring);
	if (!class_name)
		return NULL;

	char *utf8 = (char *)jni->GetStringUTFChars(&jni, class_name, NULL);
	if (!utf8)
		return NULL;

	// FalsoJNI represents a jclass as an owned copy of its class name.
	jclass clazz = jni->FindClass(&jni, utf8);
	jni->ReleaseStringUTFChars(&jni, class_name, utf8);
	return clazz;
}

static jobject getPackageName(jmethodID id, va_list args) {
	(void) id;
	(void) args;
	return jni->NewStringUTF(&jni, "com.sigmateam.zombieshooter.free");
}

static jobject getContentResolver(jmethodID id, va_list args) {
	(void) id;
	(void) args;
	/* NativeActivity inherits Context.getContentResolver(). The game passes
	 * this receiver to Settings.Secure.getString; no resolver methods are used. */
	return (jobject)0x71717171;
}

static jobject secureGetString(jmethodID id, va_list args) {
	(void) id;
	jobject resolver = va_arg(args, jobject);
	jstring name = va_arg(args, jstring);
	if (resolver != (jobject)0x71717171 || !name)
		return NULL;
	const char *key = jni->GetStringUTFChars(&jni, name, NULL);
	if (!key)
		return NULL;
	int is_android_id = strcmp(key, "android_id") == 0;
	jni->ReleaseStringUTFChars(&jni, name, (char *)key);
	if (!is_android_id)
		return NULL;
	/* Settings.Secure.ANDROID_ID is a stable 64-bit hexadecimal identifier.
	 * Keep it stable across runs so encrypted local saves keep the same key. */
	return jni->NewStringUTF(&jni, "a1b2c3d4e5f60718");
}

static jobject getVersion(jmethodID id, va_list args) {
	(void) id;
	(void) args;
	return jni->NewStringUTF(&jni, "3.5.3");
}

static jint getVersionCode(jmethodID id, va_list args) {
	(void) id;
	(void) args;
	return 1153;
}

static void quit(jmethodID id, va_list args) {
	(void) id;
	(void) args;
	/*
	 * NativeActivity owns the process lifetime in this port.  The Android Java
	 * activity method only finishes the host activity; Vita users leave through
	 * the system UI, so resolving the method and recording the request is enough.
	 */
	fjni_log_info("[JNI] Java quit() requested");
}

/* APK classes4.dex and the canonical SO confirm the helper's native callback.
 * Reproduce this small Java loop directly; no speculative List/MotionRange VM.
 * Keep class, descriptor and static/instance matching separate from legacy names. */
#define INPUT_DEVICE_CLASS "android/view/InputDevice"
#define INPUT_HELPER_CLASS "com/sigmateam/sige/InputDeviceHelper"
#define GAMEPAD_SOURCES (AINPUT_SOURCE_GAMEPAD | AINPUT_SOURCE_JOYSTICK | AINPUT_SOURCE_DPAD)
static uint32_t virtual_gamepad; // Stable borrowed object; never free as a global ref.
typedef void (*AxisInfoCallback)(JNIEnv *, jclass, jint, jfloat, jfloat, jfloat);
static AxisInfoCallback axis_info_callback;

static int class_matches(jclass clazz, const char *expected) {
    if (!clazz || clazz == (jclass)0x42424242) return 0;
    const char *actual = (const char *)clazz;
    for (; *expected; ++expected, ++actual) {
        if (*actual != *expected && !(*expected == '/' && *actual == '.')) return 0;
    }
    return *actual == 0;
}

/* Canonical SO screenSizeInInches at 0x3ca1e4 reads only xdpi:F/ydpi:F.
 * EGL already reports Vita's 220 DPI. This preserves the engine's own
 * diagonal-based scale selection, rather than imposing a work resolution. */
#define METRICS_CLASS "android/util/DisplayMetrics"
static uint32_t window_manager, default_display;
static jobject metricsInit(jmethodID id, va_list args) {
    (void)id; (void)args;
    return calloc(1, sizeof(uint32_t));
}
static jobject getWindowManager(jmethodID id, va_list args) {
    (void)id; (void)args; return (jobject)&window_manager;
}
static jobject getDefaultDisplay(jmethodID id, va_list args) {
    (void)id; (void)args; return (jobject)&default_display;
}
static void displayGetMetrics(jmethodID id, va_list args) {
    (void)id;
    jobject metrics = va_arg(args, jobject);
    if (!metrics) return;
    static unsigned reported;
    if (!__atomic_exchange_n(&reported, 1, __ATOMIC_RELAXED))
        l_perf("display_metrics xdpi=220 ydpi=220 native_width=960 native_height=544 engine_selects_work_size=1");
}
int fjni_resolve_field(jclass clazz, const char *name, const char *sig, jfieldID *result) {
    if (!class_matches(clazz, METRICS_CLASS)) {
        if (!strcmp(name,"xdpi") || !strcmp(name,"ydpi")) { *result=NULL; return 1; }
        return 0;
    }
    *result = NULL;
    if (!strcmp(sig, "F")) {
        if (!strcmp(name, "xdpi")) *result = (jfieldID)(uintptr_t)1100;
        if (!strcmp(name, "ydpi")) *result = (jfieldID)(uintptr_t)1101;
    }
    return 1;
}

int fjni_borrowed_object(jobject obj) { return obj == (jobject)&virtual_gamepad || obj == (jobject)&window_manager || obj == (jobject)&default_display; }
jclass fjni_object_class(jobject obj) {
    if (obj == (jobject)&window_manager) return jni->FindClass(&jni, "android/view/WindowManager");
    if (obj == (jobject)&default_display) return jni->FindClass(&jni, "android/view/Display");
    return obj == (jobject)&virtual_gamepad ? jni->FindClass(&jni, INPUT_DEVICE_CLASS) : NULL;
}

static jobject inputGetDevice(jmethodID id, va_list args) {
    (void)id;
    return va_arg(args, jint) == FNDK_GAMEPAD_DEVICE_ID ? (jobject)&virtual_gamepad : NULL;
}
static jint inputGetId(jmethodID id, va_list args) { (void)id; (void)args; return FNDK_GAMEPAD_DEVICE_ID; }
static jint inputGetSources(jmethodID id, va_list args) { (void)id; (void)args; return GAMEPAD_SOURCES; }
static jobject inputGetDeviceIds(jmethodID id, va_list args) {
    (void)id; (void)args;
    jintArray ids = jni->NewIntArray(&jni, 1);
    if (ids) {
        jint device = FNDK_GAMEPAD_DEVICE_ID;
        jni->SetIntArrayRegion(&jni, ids, 0, 1, &device);
    }
    return ids;
}
static void inputGetMotionRanges(jmethodID id, va_list args) {
    (void)id;
    jobject device = va_arg(args, jobject);
    if (device != (jobject)&virtual_gamepad || !axis_info_callback) return;
    static const jint axes[] = {
        AMOTION_EVENT_AXIS_X, AMOTION_EVENT_AXIS_Y,
        AMOTION_EVENT_AXIS_Z, AMOTION_EVENT_AXIS_RZ,
        AMOTION_EVENT_AXIS_HAT_X, AMOTION_EVENT_AXIS_HAT_Y,
        AMOTION_EVENT_AXIS_LTRIGGER, AMOTION_EVENT_AXIS_RTRIGGER,
        AMOTION_EVENT_AXIS_BRAKE, AMOTION_EVENT_AXIS_GAS,
    };
    for (unsigned i = 0; i < sizeof(axes) / sizeof(axes[0]); ++i)
        // The real Java helper clamps flat+fuzz to at least 0.1f.
        axis_info_callback(&jni, (jclass)INPUT_HELPER_CLASS, axes[i], i < 6 ? -1.f : 0.f, 1.f, 0.1f);
}

jint fjni_register_natives(jclass clazz, const JNINativeMethod *methods, jint count) {
    if (!class_matches(clazz, INPUT_HELPER_CLASS)) return JNI_OK;
    if (count < 0 || (count && !methods)) return JNI_ERR;
    for (jint i = 0; i < count; ++i) {
        if (strcmp(methods[i].name, "onAxisInfo") || strcmp(methods[i].signature, "(IFFF)V") || !methods[i].fnPtr)
            return JNI_ERR;
    }
    for (jint i = 0; i < count; ++i) axis_info_callback = (AxisInfoCallback)methods[i].fnPtr;
#ifdef ZOMBIE_DEBUG_BUILD
    fjni_log_info("[INPUT] InputDeviceHelper.onAxisInfo(IFFF)V registered");
#endif
    return JNI_OK;
}
void fjni_unregister_natives(jclass clazz) {
    if (class_matches(clazz, INPUT_HELPER_CLASS)) axis_info_callback = NULL;
}

typedef struct {
    const char *clazz, *name, *signature;
    jboolean is_static;
    int id;
} InputMethod;
static const InputMethod input_methods[] = {
    { METRICS_CLASS, "<init>", "()V", JNI_FALSE, METHOD_DISPLAY_METRICS_INIT },
    { "android/app/NativeActivity", "getWindowManager", "()Landroid/view/WindowManager;", JNI_FALSE, METHOD_GET_WINDOW_MANAGER },
    { "android/app/Activity", "getWindowManager", "()Landroid/view/WindowManager;", JNI_FALSE, METHOD_GET_WINDOW_MANAGER },
    { "android/view/WindowManager", "getDefaultDisplay", "()Landroid/view/Display;", JNI_FALSE, METHOD_GET_DEFAULT_DISPLAY },
    { "android/view/Display", "getMetrics", "(Landroid/util/DisplayMetrics;)V", JNI_FALSE, METHOD_DISPLAY_GET_METRICS },
    { INPUT_DEVICE_CLASS, "getDevice", "(I)Landroid/view/InputDevice;", JNI_TRUE, METHOD_INPUT_DEVICE_GET_DEVICE },
    { INPUT_DEVICE_CLASS, "getId", "()I", JNI_FALSE, METHOD_INPUT_DEVICE_GET_ID },
    { INPUT_DEVICE_CLASS, "getSources", "()I", JNI_FALSE, METHOD_INPUT_DEVICE_GET_SOURCES },
    { INPUT_DEVICE_CLASS, "getDeviceIds", "()[I", JNI_TRUE, METHOD_INPUT_DEVICE_GET_DEVICE_IDS },
    { INPUT_HELPER_CLASS, "getInputSources", "()I", JNI_TRUE, METHOD_INPUT_HELPER_GET_SOURCES },
    { INPUT_HELPER_CLASS, "getMotionRanges", "(Landroid/view/InputDevice;)V", JNI_TRUE, METHOD_INPUT_HELPER_GET_RANGES },
};

int fjni_resolve_method(jclass clazz, const char *name, const char *sig, jboolean is_static, jmethodID *result) {
    if (clazz == (jclass)0x42424242 && !strcmp(name, "getWindowManager") &&
        !strcmp(sig, "()Landroid/view/WindowManager;") && !is_static) {
        *result = (jmethodID)(uintptr_t)METHOD_GET_WINDOW_MANAGER; return 1;
    }
    for (unsigned i = 0; i < sizeof(input_methods) / sizeof(input_methods[0]); ++i) {
        const InputMethod *m = &input_methods[i];
        if (class_matches(clazz, m->clazz) && !strcmp(name, m->name) && !strcmp(sig, m->signature) && is_static == m->is_static) {
            *result = (jmethodID)(uintptr_t)m->id;
#ifdef ZOMBIE_DEBUG_BUILD
            static unsigned seen;
            unsigned bit = 1u << i;
            if (!(__atomic_fetch_or(&seen, bit, __ATOMIC_RELAXED) & bit))
                fjni_logv_info("[INPUT] JNI resolved %s.%s%s static=%d", m->clazz, name, sig, is_static);
#endif
            return 1;
        }
    }
    // Reject unsupported signatures in these classes, never bind by name alone.
    int related = class_matches(clazz, METRICS_CLASS) || class_matches(clazz, "android/view/WindowManager") ||
        class_matches(clazz, "android/view/Display") || class_matches(clazz, INPUT_DEVICE_CLASS) || class_matches(clazz, INPUT_HELPER_CLASS) ||
        class_matches(clazz, "android/view/InputDevice$MotionRange") ||
        class_matches(clazz, "com/sigmateam/sige/RegistryEnumerator");
    if (!related) return 0;
    *result = NULL;
#ifdef ZOMBIE_DEBUG_BUILD
    // At most 24 distinct diagnostics per run; do not re-enable JNI spam in Release.
    static unsigned char diagnostic_lock;
    while (__atomic_test_and_set(&diagnostic_lock, __ATOMIC_ACQUIRE)) {}
    static uint32_t hashes[24];
    static unsigned count;
    uint32_t hash = 2166136261u;
    const char *parts[] = { (const char *)clazz, name, sig };
    for (unsigned i = 0; i < 3; ++i)
        for (const unsigned char *p = (const unsigned char *)parts[i]; *p; ++p) hash = (hash ^ *p) * 16777619u;
    hash ^= is_static;
    unsigned i;
    for (i = 0; i < count; ++i) if (hashes[i] == hash) break;
    if (i == count && count < 24) {
        hashes[count++] = hash;
        fjni_logv_warn("[INPUT] JNI unresolved %s.%s%s static=%d", (const char *)clazz, name, sig, is_static);
    }
    __atomic_clear(&diagnostic_lock, __ATOMIC_RELEASE);
#endif
    return 1;
}

NameToMethodID nameToMethodId[] = {
	{ METHOD_GET_PACKAGE_NAME, "getPackageName", METHOD_TYPE_OBJECT },
	{ METHOD_GET_VERSION, "getVersion", METHOD_TYPE_OBJECT },
	{ METHOD_GET_VERSION_CODE, "getVersionCode", METHOD_TYPE_INT },
	{ METHOD_GET_CLASS_LOADER, "getClassLoader", METHOD_TYPE_OBJECT },
	{ METHOD_LOAD_CLASS, "loadClass", METHOD_TYPE_OBJECT },
	{ METHOD_QUIT, "quit", METHOD_TYPE_VOID },
	{ METHOD_GET_CONTENT_RESOLVER, "getContentResolver", METHOD_TYPE_OBJECT },
	{ METHOD_SECURE_GET_STRING, "getString", METHOD_TYPE_OBJECT },
};

MethodsBoolean methodsBoolean[] = {};
MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};
MethodsFloat methodsFloat[] = {};
MethodsInt methodsInt[] = {
	{ METHOD_GET_VERSION_CODE, getVersionCode },
    { METHOD_INPUT_DEVICE_GET_ID, inputGetId },
    { METHOD_INPUT_DEVICE_GET_SOURCES, inputGetSources },
    { METHOD_INPUT_HELPER_GET_SOURCES, inputGetSources },
};
MethodsLong methodsLong[] = {};
MethodsObject methodsObject[] = {
    { METHOD_DISPLAY_METRICS_INIT, metricsInit },
    { METHOD_GET_WINDOW_MANAGER, getWindowManager },
    { METHOD_GET_DEFAULT_DISPLAY, getDefaultDisplay },
	{ METHOD_GET_PACKAGE_NAME, getPackageName },
	{ METHOD_GET_VERSION, getVersion },
	{ METHOD_GET_CLASS_LOADER, getClassLoader },
	{ METHOD_LOAD_CLASS, loadClass },
	{ METHOD_GET_CONTENT_RESOLVER, getContentResolver },
	{ METHOD_SECURE_GET_STRING, secureGetString },
    { METHOD_INPUT_DEVICE_GET_DEVICE, inputGetDevice },
    { METHOD_INPUT_DEVICE_GET_DEVICE_IDS, inputGetDeviceIds },
};
MethodsShort methodsShort[] = {};
MethodsVoid methodsVoid[] = {
    { METHOD_DISPLAY_GET_METRICS, displayGetMetrics },
	{ METHOD_QUIT, quit },
    { METHOD_INPUT_HELPER_GET_RANGES, inputGetMotionRanges },
};

/*
 * JNI Fields
*/

// System-wide constant that applications sometimes request
// https://developer.android.com/reference/android/content/Context.html#WINDOW_SERVICE
char WINDOW_SERVICE[] = "window";

// System-wide constant that's often used to determine Android version
// https://developer.android.com/reference/android/os/Build.VERSION.html#SDK_INT
// Possible values: https://developer.android.com/reference/android/os/Build.VERSION_CODES
const int SDK_INT = 24; // Matches the APK's minSdk and native build target.

/* JNI NULL (0) is reserved for an unknown field. These IDs are opaque:
 * native callers obtain them by name, never by a hardcoded numeric value. */
enum {
    FIELD_WINDOW_SERVICE = 1000,
    FIELD_SDK_INT = 1001,
};

NameToFieldID nameToFieldId[] = {
        { 1100, "xdpi", FIELD_TYPE_FLOAT },
        { 1101, "ydpi", FIELD_TYPE_FLOAT },
		{ FIELD_WINDOW_SERVICE, "WINDOW_SERVICE", FIELD_TYPE_OBJECT },
		{ FIELD_SDK_INT, "SDK_INT", FIELD_TYPE_INT },
};

FieldsBoolean fieldsBoolean[] = {};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = { { 1100, 220.f }, { 1101, 220.f } };
FieldsInt fieldsInt[] = {
		{ FIELD_SDK_INT, SDK_INT },
};
FieldsObject fieldsObject[] = {
		{ FIELD_WINDOW_SERVICE, WINDOW_SERVICE },
};
FieldsLong fieldsLong[] = {};
FieldsShort fieldsShort[] = {};

__FALSOJNI_IMPL_CONTAINER_SIZES
