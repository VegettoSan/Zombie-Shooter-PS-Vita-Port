/* SIGE string registry storage. Preserve opaque engine ciphertext unchanged.
 * A temporary file is fully written and synchronized before rotating the last
 * known valid store. Never unlink the only valid copy on replacement failure. */
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef ZOMBIE_VITA_BUILD
#include <psp2/io/fcntl.h>
#endif
#define PREFS_BAK DATA_PATH "shared_preferences.bak"
#ifdef ZOMBIE_DEBUG_BUILD
static unsigned save_trace_count;
#define SAVE_TRACE(...) do { if (__atomic_fetch_add(&save_trace_count,1,__ATOMIC_RELAXED)<512) l_perf(__VA_ARGS__); } while(0)
#else
#define SAVE_TRACE(...) ((void)0)
#endif
static unsigned pref_key_hash(const char *s) {
    unsigned h=2166136261u;while(*s) { h^=(unsigned char)*s++;h*=16777619u; }return h;
}
static void pref_trace_read(const char *k,int found) {
#ifdef ZOMBIE_DEBUG_BUILD
    static unsigned reads;
    if(__atomic_fetch_add(&reads,1,__ATOMIC_RELAXED)<64)
        l_perf("[SAVE] preference get hash=%08X hit=%d",pref_key_hash(k),found);
#else
    (void)k;(void)found;
#endif
}
typedef struct { char *key,*value; } PrefEntry;
static PrefEntry *pref_entries;
static unsigned pref_count,pref_capacity;
static int pref_loaded,pref_dirty;
static unsigned char pref_lock_byte;
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
static int pref_read_store(const char *path) {
    FILE*f=fopen(path,"rb");
    SAVE_TRACE("[SAVE] open path=%s mode=rb ok=%d errno=%d",path,f!=NULL,f?0:errno);
    if(!f)return 0;
    char magic[8];uint32_t count=0;
    int ok=fread(magic,1,8,f)==8&&!memcmp(magic,"ZSPREF1",8)&&read_u32(f,&count)&&count<=4096;
    for(uint32_t i=0;ok&&i<count;i++) {
        uint32_t kn=0,vn=0;
        ok=read_u32(f,&kn)&&read_u32(f,&vn)&&kn&&kn<=4096&&vn<=(1u<<20);
        char*k=ok?(char*)malloc(kn+1):NULL,*v=ok?(char*)malloc(vn+1):NULL;
        if(!k||!v)ok=0;
        if(ok)ok=fread(k,1,kn,f)==kn&&fread(v,1,vn,f)==vn;
        if(!ok){free(k);free(v);break;}
        k[kn]=v[vn]=0;
        /* JNI strings cannot contain embedded NUL. Refuse truncation/duplicates. */
        if(memchr(k,0,kn)||memchr(v,0,vn)||pref_find(k)>=0||!pref_grow()) {
            free(k);free(v);ok=0;break;
        }
        pref_entries[pref_count++]=(PrefEntry){k,v};
    }
    if(ok && fgetc(f)!=EOF)ok=0;
    if(ferror(f))ok=0;
    if(fclose(f))ok=0;
    if(!ok) { pref_free_all();l_warn("[SAVE] invalid store path=%s",path); }
    SAVE_TRACE("[SAVE] read path=%s valid=%d keys=%u",path,ok,pref_count);
    return ok;
}
static int pref_current_valid;
static void pref_load(void) {
    if(pref_loaded)return;
    pref_loaded=1;
    pref_current_valid=pref_read_store(PREFS_FILE);
    if(!pref_current_valid && pref_read_store(PREFS_BAK)) {
        SAVE_TRACE("[SAVE] recovered backup keys=%u",pref_count);
    }
    pref_dirty=0;
}
static int pref_sync(FILE *f) {
    if(fflush(f))return 0;
#ifdef ZOMBIE_VITA_BUILD
    /* newlib FILE descriptors and sceIo descriptors are different namespaces.
     * Do not feed fileno(FILE*) to sceIoSyncByFd. Flush libc then sync ux0. */
    int ret=sceIoSync("ux0:",0);
#else
    int ret=fsync(fileno(f));
#endif
    SAVE_TRACE("[SAVE] fsync path=%s result=%d errno=%d",PREFS_TMP,ret,ret?errno:0);
    return ret==0;
}
/* Injectable only in the standalone regression translation unit. */
#ifndef PREF_RENAME
#define PREF_RENAME rename
#endif
static int pref_save(void) {
    pref_load();if(!pref_dirty)return 1;
    FILE*f=fopen(PREFS_TMP,"wb");
    SAVE_TRACE("[SAVE] create path=%s ok=%d errno=%d",PREFS_TMP,f!=NULL,f?0:errno);
    if(!f)return 0;
    int ok=fwrite("ZSPREF1",1,8,f)==8&&write_u32(f,pref_count);
    for(unsigned i=0;ok&&i<pref_count;i++) {
        uint32_t kn=(uint32_t)strlen(pref_entries[i].key),vn=(uint32_t)strlen(pref_entries[i].value);
        ok=write_u32(f,kn)&&write_u32(f,vn)&&fwrite(pref_entries[i].key,1,kn,f)==kn&&fwrite(pref_entries[i].value,1,vn,f)==vn;
    }
    SAVE_TRACE("[SAVE] write path=%s keys=%u ok=%d",PREFS_TMP,pref_count,ok);
    if(ok)ok=pref_sync(f);
    if(fclose(f))ok=0;
    if(!ok){remove(PREFS_TMP);l_error("[SAVE] write/sync failed errno=%d",errno);return 0;}
    int rotated=0;
    if(pref_current_valid) {
        /* The current file is validated; removing an older backup is safe. */
        if(remove(PREFS_BAK) && errno!=ENOENT)ok=0;
        if(ok)ok=PREF_RENAME(PREFS_FILE,PREFS_BAK)==0;
        rotated=ok;
        SAVE_TRACE("[SAVE] rename from=%s to=%s ok=%d errno=%d",PREFS_FILE,PREFS_BAK,ok,ok?0:errno);
    } else {
        /* A corrupt main must not block rename on Vita; preserve the backup. */
        if(remove(PREFS_FILE) && errno!=ENOENT)ok=0;
    }
    if(ok) {
        ok=PREF_RENAME(PREFS_TMP,PREFS_FILE)==0;
        SAVE_TRACE("[SAVE] rename from=%s to=%s ok=%d errno=%d",PREFS_TMP,PREFS_FILE,ok,ok?0:errno);
    }
    if(!ok) {
        if(rotated) {
            int restored=PREF_RENAME(PREFS_BAK,PREFS_FILE)==0;
            pref_current_valid=restored;
            SAVE_TRACE("[SAVE] restore backup ok=%d errno=%d",restored,restored?0:errno);
        }
        remove(PREFS_TMP);l_error("[SAVE] replacement failed; previous store retained errno=%d",errno);return 0;
    }
    pref_current_valid=1;
#ifdef ZOMBIE_VITA_BUILD
    int sync_result=sceIoSync("ux0:",0);
#else
    int directory=open(DATA_PATH ".",O_RDONLY|O_DIRECTORY);
    int sync_result=directory<0?-1:fsync(directory);
    if(directory>=0)close(directory);
#endif
    SAVE_TRACE("[SAVE] fsync after rename result=%d",sync_result);
    if(sync_result) { l_error("[SAVE] post-rename sync failed; backup retained");return 0; }
    pref_dirty=0;
    /* Keep the previous valid backup. A damaged next-boot main can recover it. */
    return 1;
}
