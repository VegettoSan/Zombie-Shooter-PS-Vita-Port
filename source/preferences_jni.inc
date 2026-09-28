#include <stdio.h>
#ifndef DATA_PATH
#define DATA_PATH ""
#endif
#define PREFS_CLASS "android/content/SharedPreferences"
#define PREFS_EDITOR_CLASS "android/content/SharedPreferences$Editor"
#define REGISTRY_ENUM_CLASS "com/sigmateam/sige/RegistryEnumerator"
enum { METHOD_PREFS_ACTIVITY=400, METHOD_PREFS_GET, METHOD_PREFS_CONTAINS, METHOD_PREFS_EDIT,
       METHOD_PREFS_PUT, METHOD_PREFS_REMOVE, METHOD_PREFS_CLEAR, METHOD_PREFS_COMMIT,
       METHOD_PREFS_APPLY, METHOD_PREFS_ENUM };
static uint32_t prefs_object, prefs_editor_object;
typedef void (*RegistryKeyCallback)(JNIEnv *, jclass, jstring);
static RegistryKeyCallback registry_key_callback;
#include "preferences_store.inc"

static jobject activity_get_preferences(jmethodID id,va_list a){
    (void)id;(void)va_arg(a,jint);
    pref_lock();pref_load();
    SAVE_TRACE("[SAVE] Activity.getPreferences keys=%u path=%s",pref_count,PREFS_DIR);
    pref_unlock();
    return(jobject)&prefs_object;
}

static jobject prefs_get_string(jmethodID id,va_list a){
    (void)id;
    jstring ks=va_arg(a,jstring),fallback=va_arg(a,jstring);
    if(!ks)return fallback;
    const char*k=jni->GetStringUTFChars(&jni,ks,NULL);
    if(!k)return fallback;
    char*v=NULL;
    pref_lock();pref_load();
    int x=pref_find(k);
    pref_trace_read(k,x>=0);
    if(x>=0)v=pref_dup(pref_entries[x].value,strlen(pref_entries[x].value));
    pref_unlock();
    jni->ReleaseStringUTFChars(&jni,ks,(char*)k);
    if(!v)return fallback;
    jstring out=jni->NewStringUTF(&jni,v);
    free(v);
    return out;
}

static jboolean prefs_contains(jmethodID id,va_list a){
    (void)id;
    jstring s=va_arg(a,jstring);
    if(!s)return JNI_FALSE;
    const char*k=jni->GetStringUTFChars(&jni,s,NULL);
    if(!k)return JNI_FALSE;
    pref_lock();pref_load();int found=pref_find(k)>=0;pref_unlock();
    jni->ReleaseStringUTFChars(&jni,s,(char*)k);
    return found?JNI_TRUE:JNI_FALSE;
}

static jobject prefs_edit(jmethodID id,va_list a){
    (void)id;(void)a;
    pref_lock();pref_load();pref_unlock();
    return(jobject)&prefs_editor_object;
}

static jobject prefs_put_string(jmethodID id,va_list a){
    (void)id;
    jstring ks=va_arg(a,jstring),vs=va_arg(a,jstring);
    if(!ks)return(jobject)&prefs_editor_object;
    const char*k=jni->GetStringUTFChars(&jni,ks,NULL);
    const char*v=vs?jni->GetStringUTFChars(&jni,vs,NULL):NULL;
    if(k){
        int ok=1,changed=0;
        pref_lock();pref_load();
        if(v){
            int x=pref_find(k);
            changed=x<0||strcmp(pref_entries[x].value,v)!=0;
            if(changed){ok=pref_set(k,v);if(ok)ok=pref_persist_current_key(k);}
        }else{
            changed=pref_find(k)>=0;
            (void)pref_remove_key(k);
            ok=pref_delete_key_file(k);
            if(ok)pref_dirty=0;
        }
        SAVE_TRACE("[SAVE] preference put-one hash=%08X bytes=%u changed=%d ok=%d",
                   pref_key_hash(k),v?(unsigned)strlen(v):0,changed,ok);
        if(!ok)l_error("[SAVE] preference incremental write failed");
        pref_unlock();
    }
    if(v)jni->ReleaseStringUTFChars(&jni,vs,(char*)v);
    if(k)jni->ReleaseStringUTFChars(&jni,ks,(char*)k);
    return(jobject)&prefs_editor_object;
}

static jobject prefs_remove(jmethodID id,va_list a){
    (void)id;
    jstring s=va_arg(a,jstring);
    if(s){
        const char*k=jni->GetStringUTFChars(&jni,s,NULL);
        if(k){
            pref_lock();pref_load();
            int existed=pref_find(k)>=0;
            (void)pref_remove_key(k);
            int ok=pref_delete_key_file(k);
            if(ok)pref_dirty=0;
            SAVE_TRACE("[SAVE] preference remove-one hash=%08X existed=%d ok=%d",
                       pref_key_hash(k),existed,ok);
            pref_unlock();
            jni->ReleaseStringUTFChars(&jni,s,(char*)k);
        }
    }
    return(jobject)&prefs_editor_object;
}

static jobject prefs_clear(jmethodID id,va_list a){
    (void)id;(void)a;
    pref_lock();pref_load();
    int ok=pref_clear_files();
    pref_free_all();
    pref_dirty=ok?0:1;
    SAVE_TRACE("[SAVE] preference clear files ok=%d",ok);
    pref_unlock();
    return(jobject)&prefs_editor_object;
}

static jboolean prefs_commit(jmethodID id,va_list a){
    (void)id;(void)a;
    pref_lock();int ok=pref_save();
    SAVE_TRACE("[SAVE] commit incremental ok=%d keys=%u",ok,pref_count);
    pref_unlock();
    return ok?JNI_TRUE:JNI_FALSE;
}

static void prefs_apply(jmethodID id,va_list a){
    (void)id;(void)a;
    pref_lock();int ok=pref_save();
    SAVE_TRACE("[SAVE] apply incremental ok=%d keys=%u",ok,pref_count);
    pref_unlock();
}

static void registry_enumerate(jmethodID id,va_list a){
    (void)id;(void)va_arg(a,jobject);
    SAVE_TRACE("[SAVE] RegistryEnumerator callback=%d",registry_key_callback!=NULL);
    if(!registry_key_callback)return;
    char**keys=NULL;unsigned n=0;
    pref_lock();pref_load();
    if(pref_count)keys=(char**)calloc(pref_count,sizeof(*keys));
    if(keys)for(;n<pref_count;n++){
        keys[n]=pref_dup(pref_entries[n].key,strlen(pref_entries[n].key));
        if(!keys[n])break;
    }
    pref_unlock();
    for(unsigned i=0;i<n;i++){
        jstring s=jni->NewStringUTF(&jni,keys[i]);
        if(s){registry_key_callback(&jni,(jclass)REGISTRY_ENUM_CLASS,s);jni->DeleteGlobalRef(&jni,s);}
        free(keys[i]);
    }
    free(keys);
}
