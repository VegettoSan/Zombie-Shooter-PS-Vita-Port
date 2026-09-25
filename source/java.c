#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_Impl.h>
#include <falso_jni/FalsoJNI_Logger.h>
#include <string.h>

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
};
MethodsLong methodsLong[] = {};
MethodsObject methodsObject[] = {
	{ METHOD_GET_PACKAGE_NAME, getPackageName },
	{ METHOD_GET_VERSION, getVersion },
	{ METHOD_GET_CLASS_LOADER, getClassLoader },
	{ METHOD_LOAD_CLASS, loadClass },
	{ METHOD_GET_CONTENT_RESOLVER, getContentResolver },
	{ METHOD_SECURE_GET_STRING, secureGetString },
};
MethodsShort methodsShort[] = {};
MethodsVoid methodsVoid[] = {
	{ METHOD_QUIT, quit },
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

NameToFieldID nameToFieldId[] = {
		{ 0, "WINDOW_SERVICE", FIELD_TYPE_OBJECT }, 
		{ 1, "SDK_INT", FIELD_TYPE_INT },
};

FieldsBoolean fieldsBoolean[] = {};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = {};
FieldsInt fieldsInt[] = {
		{ 1, SDK_INT },
};
FieldsObject fieldsObject[] = {
		{ 0, WINDOW_SERVICE },
};
FieldsLong fieldsLong[] = {};
FieldsShort fieldsShort[] = {};

__FALSOJNI_IMPL_CONTAINER_SIZES
