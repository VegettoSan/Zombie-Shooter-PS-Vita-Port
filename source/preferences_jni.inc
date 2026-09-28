#include <stdio.h>
#ifndef DATA_PATH
#define DATA_PATH ""
#endif
#define PREFS_CLASS "android/content/SharedPreferences"
#define PREFS_EDITOR_CLASS "android/content/SharedPreferences$Editor"
#define REGISTRY_ENUM_CLASS "com/sigmateam/sige/RegistryEnumerator"
#define PREFS_FILE DATA_PATH "shared_preferences.bin"
#define PREFS_TMP  DATA_PATH "shared_preferences.tmp"
enum { METHOD_PREFS_ACTIVITY=400, METHOD_PREFS_GET, METHOD_PREFS_CONTAINS, METHOD_PREFS_EDIT,
       METHOD_PREFS_PUT, METHOD_PREFS_REMOVE, METHOD_PREFS_CLEAR, METHOD_PREFS_COMMIT,
       METHOD_PREFS_APPLY, METHOD_PREFS_ENUM };
static uint32_t prefs_object, prefs_editor_object;
typedef void (*RegistryKeyCallback)(JNIEnv *, jclass, jstring);
static RegistryKeyCallback registry_key_callback;
typedef struct { char *key,*value; } PrefEntry;
static PrefEntry *pref_entries; static unsigned pref_count,pref_capacity; static int pref_loaded,pref_dirty; static unsigned char pref_lock_byte;
static void pref_lock(void){while(__atomic_test_and_set(&pref_lock_byte,__ATOMIC_ACQUIRE)){}}
static void pref_unlock(void){__atomic_clear(&pref_lock_byte,__ATOMIC_RELEASE);}
static char *pref_dup(const char*s,size_t n){char*p=(char*)malloc(n+1);if(p){if(n)memcpy(p,s,n);p[n]=0;}return p;}
static int pref_find(const char*k){for(unsigned i=0;i<pref_count;i++)if(!strcmp(pref_entries[i].key,k))return(int)i;return-1;}
static int pref_grow(void){if(pref_count<pref_capacity)return 1;unsigned n=pref_capacity?pref_capacity*2:32;PrefEntry*p=(PrefEntry*)realloc(pref_entries,n*sizeof(*p));if(!p)return 0;pref_entries=p;pref_capacity=n;return 1;}
static void pref_free_all(void){for(unsigned i=0;i<pref_count;i++){free(pref_entries[i].key);free(pref_entries[i].value);}free(pref_entries);pref_entries=NULL;pref_count=pref_capacity=0;}
static int pref_set(const char*k,const char*v){size_t kn=strlen(k),vn=strlen(v);if(!kn||kn>4096||vn>(1u<<20))return 0;char*nv=pref_dup(v,vn);if(!nv)return 0;int x=pref_find(k);if(x>=0){free(pref_entries[x].value);pref_entries[x].value=nv;pref_dirty=1;return 1;}char*nk=pref_dup(k,kn);if(!nk||pref_count>=4096||!pref_grow()){free(nk);free(nv);return 0;}pref_entries[pref_count++]=(PrefEntry){nk,nv};pref_dirty=1;return 1;}
static void pref_remove_key(const char*k){int x=pref_find(k);if(x<0)return;free(pref_entries[x].key);free(pref_entries[x].value);pref_entries[x]=pref_entries[--pref_count];pref_dirty=1;}
static int read_u32(FILE*f,uint32_t*v){unsigned char b[4];if(fread(b,1,4,f)!=4)return 0;*v=b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);return 1;}
static int write_u32(FILE*f,uint32_t v){unsigned char b[4]={(unsigned char)v,(unsigned char)(v>>8),(unsigned char)(v>>16),(unsigned char)(v>>24)};return fwrite(b,1,4,f)==4;}
static void pref_load(void){if(pref_loaded)return;pref_loaded=1;FILE*f=fopen(PREFS_FILE,"rb");if(!f)return;char magic[8];uint32_t count=0;int ok=fread(magic,1,8,f)==8&&!memcmp(magic,"ZSPREF1",8)&&read_u32(f,&count)&&count<=4096;for(uint32_t i=0;ok&&i<count;i++){uint32_t kn=0,vn=0;ok=read_u32(f,&kn)&&read_u32(f,&vn)&&kn&&kn<=4096&&vn<=(1u<<20);char*k=ok?(char*)malloc(kn+1):NULL,*v=ok?(char*)malloc(vn+1):NULL;if(!k||!v)ok=0;if(ok)ok=fread(k,1,kn,f)==kn&&fread(v,1,vn,f)==vn;if(!ok){free(k);free(v);break;}k[kn]=v[vn]=0;if(!pref_grow()){free(k);free(v);ok=0;break;}pref_entries[pref_count++]=(PrefEntry){k,v};}fclose(f);if(!ok){pref_free_all();l_warn("[PREFS] corrupt store ignored");}else l_info("[PREFS] loaded %u keys",pref_count);pref_dirty=0;}
static int pref_save(void){pref_load();if(!pref_dirty)return 1;FILE*f=fopen(PREFS_TMP,"wb");if(!f)return 0;int ok=fwrite("ZSPREF1",1,8,f)==8&&write_u32(f,pref_count);for(unsigned i=0;ok&&i<pref_count;i++){uint32_t kn=(uint32_t)strlen(pref_entries[i].key),vn=(uint32_t)strlen(pref_entries[i].value);ok=write_u32(f,kn)&&write_u32(f,vn)&&fwrite(pref_entries[i].key,1,kn,f)==kn&&fwrite(pref_entries[i].value,1,vn,f)==vn;}if(fclose(f))ok=0;if(ok){remove(PREFS_FILE);ok=rename(PREFS_TMP,PREFS_FILE)==0;}if(!ok){remove(PREFS_TMP);l_error("[PREFS] persist failed");return 0;}pref_dirty=0;return 1;}
static jobject activity_get_preferences(jmethodID id,va_list a){(void)id;(void)va_arg(a,jint);pref_lock();pref_load();pref_unlock();return(jobject)&prefs_object;}
static jobject prefs_get_string(jmethodID id,va_list a){(void)id;jstring ks=va_arg(a,jstring),fallback=va_arg(a,jstring);if(!ks)return fallback;const char*k=jni->GetStringUTFChars(&jni,ks,NULL);if(!k)return fallback;char*v=NULL;pref_lock();pref_load();int x=pref_find(k);if(x>=0)v=pref_dup(pref_entries[x].value,strlen(pref_entries[x].value));pref_unlock();jni->ReleaseStringUTFChars(&jni,ks,(char*)k);if(!v)return fallback;jstring out=jni->NewStringUTF(&jni,v);free(v);return out;}
static jboolean prefs_contains(jmethodID id,va_list a){(void)id;jstring s=va_arg(a,jstring);if(!s)return JNI_FALSE;const char*k=jni->GetStringUTFChars(&jni,s,NULL);if(!k)return JNI_FALSE;pref_lock();pref_load();int found=pref_find(k)>=0;pref_unlock();jni->ReleaseStringUTFChars(&jni,s,(char*)k);return found?JNI_TRUE:JNI_FALSE;}
static jobject prefs_edit(jmethodID id,va_list a){(void)id;(void)a;pref_lock();pref_load();pref_unlock();return(jobject)&prefs_editor_object;}
static jobject prefs_put_string(jmethodID id,va_list a){(void)id;jstring ks=va_arg(a,jstring),vs=va_arg(a,jstring);if(!ks)return(jobject)&prefs_editor_object;const char*k=jni->GetStringUTFChars(&jni,ks,NULL),*v=vs?jni->GetStringUTFChars(&jni,vs,NULL):NULL;if(k){pref_lock();pref_load();if(v)pref_set(k,v);else pref_remove_key(k);pref_unlock();}if(v)jni->ReleaseStringUTFChars(&jni,vs,(char*)v);if(k)jni->ReleaseStringUTFChars(&jni,ks,(char*)k);return(jobject)&prefs_editor_object;}
static jobject prefs_remove(jmethodID id,va_list a){(void)id;jstring s=va_arg(a,jstring);if(s){const char*k=jni->GetStringUTFChars(&jni,s,NULL);if(k){pref_lock();pref_load();pref_remove_key(k);pref_unlock();jni->ReleaseStringUTFChars(&jni,s,(char*)k);}}return(jobject)&prefs_editor_object;}
static jobject prefs_clear(jmethodID id,va_list a){(void)id;(void)a;pref_lock();pref_load();pref_free_all();pref_dirty=1;pref_unlock();return(jobject)&prefs_editor_object;}
static jboolean prefs_commit(jmethodID id,va_list a){(void)id;(void)a;pref_lock();int ok=pref_save();pref_unlock();return ok?JNI_TRUE:JNI_FALSE;}
static void prefs_apply(jmethodID id,va_list a){(void)id;(void)a;pref_lock();(void)pref_save();pref_unlock();}
static void registry_enumerate(jmethodID id,va_list a){(void)id;(void)va_arg(a,jobject);if(!registry_key_callback)return;char**keys=NULL;unsigned n=0;pref_lock();pref_load();if(pref_count)keys=(char**)calloc(pref_count,sizeof(*keys));if(keys)for(;n<pref_count;n++){keys[n]=pref_dup(pref_entries[n].key,strlen(pref_entries[n].key));if(!keys[n])break;}pref_unlock();for(unsigned i=0;i<n;i++){jstring s=jni->NewStringUTF(&jni,keys[i]);if(s){registry_key_callback(&jni,(jclass)REGISTRY_ENUM_CLASS,s);jni->DeleteGlobalRef(&jni,s);}free(keys[i]);}free(keys);}
