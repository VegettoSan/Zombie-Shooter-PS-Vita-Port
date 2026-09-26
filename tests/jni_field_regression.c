#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_Impl.h>

JNIEnv jni;
JavaVM jvm;
/* Logging is the only platform service replaced in this host test. */
#define LOG_STUB(name) void name(const char *fi, int li, const char *fn, const char *fmt, ...) \
    { (void)fi; (void)li; (void)fn; (void)fmt; }
LOG_STUB(_fjni_log_info)
LOG_STUB(_fjni_log_warn)
LOG_STUB(_fjni_log_debug)
LOG_STUB(_fjni_log_error)

int main(void) {
    jfieldID window = getFieldIdByName("WINDOW_SERVICE");
    jfieldID sdk = getFieldIdByName("SDK_INT");
    jfieldID signatures = getFieldIdByName("signatures");
    assert(window != NULL);
    assert(sdk != NULL && sdk != window);
    assert(strcmp(getObjectFieldValueById(window), "window") == 0);
    assert(getIntFieldValueById(sdk) == 24);
    assert(signatures == NULL);
    assert(getObjectFieldValueById(signatures) == NULL);
    assert(getObjectFieldValueById((jfieldID)9999) == NULL);
    /* The original SO skips its certificate element loop when array length
     * is unavailable. The actual array-size helper rejects NULL. */
    assert(jda_sizeof(getObjectFieldValueById(signatures)) == -1);
    /* Existing object arrays continue to work. */
    JavaDynArray *array = jda_alloc(2, FIELD_TYPE_OBJECT);
    assert(array != NULL && jda_sizeof(array) == 2);
    assert(jda_free(array) == JNI_TRUE);
    puts("JNI field regression passed: signatures absent; WINDOW_SERVICE and SDK_INT unchanged");
    return 0;
}
