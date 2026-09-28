/* Zombie Shooter Registry backing store for Vita.
 *
 * MetalSyntax-style policy: keep the engine's Registry/SharedPreferences
 * contract, but persist each logical SharedPreferences string as a normal
 * file under ux0:data/zombieshooter/save/.  There is no aggregate Vita-only
 * container and no ZSPREF1 header.  File contents are exactly the STRING bytes
 * handed to the backend.  Keys are represented reversibly as hexadecimal file
 * names so RegistryEnumerator can reconstruct the original key set safely.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef ZOMBIE_VITA_BUILD
#include <psp2/io/fcntl.h>
#endif

#define PREFS_DIR DATA_PATH "save"
#define PREFS_EXT ".dat"

#ifdef ZOMBIE_DEBUG_BUILD
static unsigned save_trace_count;
#define SAVE_TRACE(...) do { if (__atomic_fetch_add(&save_trace_count,1,__ATOMIC_RELAXED)<512) l_perf(__VA_ARGS__); } while(0)
#else
#define SAVE_TRACE(...) ((void)0)
#endif

static __attribute__((unused)) unsigned pref_key_hash(const char *s) {
    unsigned h=2166136261u;while(*s){h^=(unsigned char)*s++;h*=16777619u;}return h;
}
static void pref_trace_read(const char *k,int found) {
#ifdef ZOMBIE_DEBUG_BUILD
    static unsigned reads;
    if(__atomic_fetch_add(&reads,1,__ATOMIC_RELAXED)<128)
        l_perf("[SAVE] registry file get hash=%08X hit=%d",pref_key_hash(k),found);
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
static int pref_set(const char*k,const char*v){size_t kn=strlen(k),vn=strlen(v);if(!kn||kn>120||vn>(1u<<20))return 0;char*nv=pref_dup(v,vn);if(!nv)return 0;int x=pref_find(k);if(x>=0){free(pref_entries[x].value);pref_entries[x].value=nv;pref_dirty=1;return 1;}char*nk=pref_dup(k,kn);if(!nk||pref_count>=4096||!pref_grow()){free(nk);free(nv);return 0;}pref_entries[pref_count++]=(PrefEntry){nk,nv};pref_dirty=1;return 1;}
static void pref_remove_key(const char*k){int x=pref_find(k);if(x<0)return;free(pref_entries[x].key);free(pref_entries[x].value);pref_entries[x]=pref_entries[--pref_count];pref_dirty=1;}

static int pref_ensure_dir(void) {
#ifdef ZOMBIE_VITA_BUILD
    int ret=sceIoMkdir(PREFS_DIR,0777);
    if(ret<0 && access(PREFS_DIR,F_OK)!=0){l_error("[SAVE] mkdir failed path=%s ret=0x%08X",PREFS_DIR,(unsigned)ret);return 0;}
#else
    if(mkdir(PREFS_DIR,0777)!=0 && errno!=EEXIST){l_error("[SAVE] mkdir failed path=%s errno=%d",PREFS_DIR,errno);return 0;}
#endif
    return 1;
}

static int pref_path_for_key(char*out,size_t cap,const char*k,const char*suffix) {
    static const char hex[]="0123456789abcdef";
    size_t kn=strlen(k),base=strlen(PREFS_DIR),sn=strlen(suffix);
    if(!kn||kn>120||base+1+kn*2+sn+1>cap)return 0;
    memcpy(out,PREFS_DIR,base);out[base++]='/';
    for(size_t i=0;i<kn;i++){unsigned c=(unsigned char)k[i];out[base++]=hex[c>>4];out[base++]=hex[c&15];}
    memcpy(out+base,suffix,sn);out[base+sn]=0;return 1;
}
static int pref_hex_value(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return-1;}
static char *pref_key_from_filename(const char*name) {
    size_t n=strlen(name),ext=strlen(PREFS_EXT);if(n<=ext||strcmp(name+n-ext,PREFS_EXT))return NULL;
    n-=ext;if(!n||(n&1)||n/2>120)return NULL;char*k=(char*)malloc(n/2+1);if(!k)return NULL;
    for(size_t i=0;i<n;i+=2){int a=pref_hex_value(name[i]),b=pref_hex_value(name[i+1]);if(a<0||b<0){free(k);return NULL;}k[i/2]=(char)((a<<4)|b);}
    k[n/2]=0;if(memchr(k,0,n/2)){free(k);return NULL;}return k;
}

static int pref_read_value_file(const char *path,char **value_out) {
    FILE*f=fopen(path,"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return 0;}long sz=ftell(f);if(sz<0||sz>(1<<20)||fseek(f,0,SEEK_SET)!=0){fclose(f);return 0;}
    char*v=(char*)malloc((size_t)sz+1);if(!v){fclose(f);return 0;}
    int ok=(sz==0||fread(v,1,(size_t)sz,f)==(size_t)sz)&&!ferror(f);if(fclose(f)!=0)ok=0;
    if(!ok||memchr(v,0,(size_t)sz)){free(v);return 0;}v[sz]=0;*value_out=v;return 1;
}

static void pref_load(void) {
    if(pref_loaded) return;
    pref_loaded=1;
    if(!pref_ensure_dir()){pref_dirty=0;return;}
    DIR*d=opendir(PREFS_DIR);
    if(!d){pref_dirty=0;return;}
    struct dirent*de;
    while((de=readdir(d))!=NULL){char*k=pref_key_from_filename(de->d_name);if(!k)continue;char path[768],*v=NULL;
        if(pref_path_for_key(path,sizeof(path),k,PREFS_EXT)&&pref_read_value_file(path,&v)&&pref_find(k)<0&&pref_count<4096&&pref_grow()){
            pref_entries[pref_count++]=(PrefEntry){k,v};k=NULL;v=NULL;
        }
        free(k);free(v);
    }
    closedir(d);pref_dirty=0;SAVE_TRACE("[SAVE] loaded registry directory path=%s keys=%u",PREFS_DIR,pref_count);
}

static int pref_sync_file(FILE *f,const char *path) {
    (void)path;
    if(fflush(f))return 0;
#ifdef ZOMBIE_VITA_BUILD
    (void)f;int ret=sceIoSync("ux0:",0);
#else
    int ret=fsync(fileno(f));
#endif
    SAVE_TRACE("[SAVE] sync path=%s result=%d",path,ret);return ret==0;
}

static int pref_write_entry(const PrefEntry *e) {
    char path[768],tmp[768],bak[768];
    if(!pref_path_for_key(path,sizeof(path),e->key,PREFS_EXT)||!pref_path_for_key(tmp,sizeof(tmp),e->key,".tmp")||!pref_path_for_key(bak,sizeof(bak),e->key,".bak"))return 0;
    FILE*f=fopen(tmp,"wb");SAVE_TRACE("[SAVE] create registry file hash=%08X path=%s ok=%d",pref_key_hash(e->key),tmp,f!=NULL);if(!f)return 0;
    size_t n=strlen(e->value);int ok=(n==0||fwrite(e->value,1,n,f)==n);if(ok)ok=pref_sync_file(f,tmp);if(fclose(f)!=0)ok=0;
    if(!ok){remove(tmp);return 0;}
    int had=(access(path,F_OK)==0);if(had){remove(bak);if(rename(path,bak)!=0){remove(tmp);return 0;}}
    if(rename(tmp,path)!=0){if(had)rename(bak,path);remove(tmp);return 0;}
    if(had) remove(bak);
    return 1;
}

static int pref_remove_stale_files(void) {
    DIR*d=opendir(PREFS_DIR);if(!d)return 1;struct dirent*de;int ok=1;
    while((de=readdir(d))!=NULL){char*k=pref_key_from_filename(de->d_name);if(!k)continue;if(pref_find(k)<0){char p[768],t[768],b[768];if(pref_path_for_key(p,sizeof(p),k,PREFS_EXT)&&remove(p)!=0&&errno!=ENOENT)ok=0;if(pref_path_for_key(t,sizeof(t),k,".tmp"))remove(t);if(pref_path_for_key(b,sizeof(b),k,".bak"))remove(b);SAVE_TRACE("[SAVE] remove registry file hash=%08X ok=%d",pref_key_hash(k),ok);}free(k);}
    closedir(d);return ok;
}

static int pref_save(void) {
    pref_load();if(!pref_dirty)return 1;if(!pref_ensure_dir())return 0;int ok=pref_remove_stale_files();
    for(unsigned i=0;ok&&i<pref_count;i++)ok=pref_write_entry(&pref_entries[i]);
#ifdef ZOMBIE_VITA_BUILD
    if(ok)ok=sceIoSync("ux0:",0)==0;
#endif
    SAVE_TRACE("[SAVE] directory commit path=%s keys=%u ok=%d",PREFS_DIR,pref_count,ok);if(ok)pref_dirty=0;return ok;
}

/* Used by the 3.6.1 Registry::storeEncrypted/loadDecrypted compatibility hooks.
 * The key supplied here is SigmaTeam's own Registry::encryptKey() output, so
 * Registry::contains/remove continue to address exactly the same logical key. */
int zombie_registry_direct_put(const char *key,const char *value) {
    if(!key||!value) return 0;
    pref_lock();
    pref_load();
    int was_dirty=pref_dirty;
    int ok=pref_set(key,value);
    int x=ok?pref_find(key):-1;
    if(ok) ok=x>=0&&pref_ensure_dir()&&pref_write_entry(&pref_entries[x]);
    if(ok && !was_dirty) pref_dirty=0;
    pref_unlock();
    SAVE_TRACE("[SAVE] direct Registry put hash=%08X bytes=%u ok=%d",pref_key_hash(key),(unsigned)strlen(value),ok);
    return ok;
}
char *zombie_registry_direct_get(const char *key) {
    if(!key) return NULL;
    char*out=NULL;
    pref_lock();
    pref_load();
    int x=pref_find(key);
    if(x>=0) out=pref_dup(pref_entries[x].value,strlen(pref_entries[x].value));
    pref_trace_read(key,x>=0);
    pref_unlock();
    return out;
}
