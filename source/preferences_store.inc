/* Zombie Shooter Registry backing store for Vita.
 *
 * Keep SigmaTeam's Registry/SharedPreferences contract, but persist each
 * logical string as a normal file under ux0:data/zombieshooter/save/.
 *
 * Build 48 proved this layout on real hardware. Build 52 made mutations
 * incremental. Build 56 removed logger/device-sync stalls. This revision keeps
 * Registry reads in RAM and moves physical save-file writes/deletes/clears to
 * a dedicated Vita I/O worker with per-key coalescing, so the engine thread
 * never waits on fopen/fwrite/rename metadata. Startup reads also use direct
 * sceIo* calls on Vita to avoid stdio seek/tell overhead for every tiny .dat.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef ZOMBIE_VITA_BUILD
#include <pthread.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
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
static void pref_lock(void){while(__atomic_test_and_set(&pref_lock_byte,__ATOMIC_ACQUIRE)){ }}
static void pref_unlock(void){__atomic_clear(&pref_lock_byte,__ATOMIC_RELEASE);}
static char *pref_dup(const char*s,size_t n){char*p=(char*)malloc(n+1);if(p){if(n)memcpy(p,s,n);p[n]=0;}return p;}
static int pref_find(const char*k){for(unsigned i=0;i<pref_count;i++)if(!strcmp(pref_entries[i].key,k))return(int)i;return-1;}
static int pref_grow(void){if(pref_count<pref_capacity)return 1;unsigned n=pref_capacity?pref_capacity*2:32;PrefEntry*p=(PrefEntry*)realloc(pref_entries,n*sizeof(*p));if(!p)return 0;pref_entries=p;pref_capacity=n;return 1;}
static void pref_free_all(void){for(unsigned i=0;i<pref_count;i++){free(pref_entries[i].key);free(pref_entries[i].value);}free(pref_entries);pref_entries=NULL;pref_count=pref_capacity=0;}
static int pref_set(const char*k,const char*v){size_t kn=strlen(k),vn=strlen(v);if(!kn||kn>120||vn>(1u<<20))return 0;char*nv=pref_dup(v,vn);if(!nv)return 0;int x=pref_find(k);if(x>=0){free(pref_entries[x].value);pref_entries[x].value=nv;pref_dirty=1;return 1;}char*nk=pref_dup(k,kn);if(!nk||pref_count>=4096||!pref_grow()){free(nk);free(nv);return 0;}pref_entries[pref_count++]=(PrefEntry){nk,nv};pref_dirty=1;return 1;}
static int pref_remove_key(const char*k){int x=pref_find(k);if(x<0)return 0;free(pref_entries[x].key);free(pref_entries[x].value);pref_entries[x]=pref_entries[--pref_count];pref_dirty=1;return 1;}

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
static int pref_hex_basename(const char *name,size_t n) {
    if(!n||(n&1)||n/2>120)return 0;
    for(size_t i=0;i<n;i++)if(pref_hex_value(name[i])<0)return 0;
    return 1;
}
static char *pref_key_from_filename(const char*name) {
    size_t n=strlen(name),ext=strlen(PREFS_EXT);if(n<=ext||strcmp(name+n-ext,PREFS_EXT))return NULL;
    n-=ext;if(!pref_hex_basename(name,n))return NULL;char*k=(char*)malloc(n/2+1);if(!k)return NULL;
    for(size_t i=0;i<n;i+=2){int a=pref_hex_value(name[i]),b=pref_hex_value(name[i+1]);k[i/2]=(char)((a<<4)|b);}
    k[n/2]=0;if(memchr(k,0,n/2)){free(k);return NULL;}return k;
}
static int pref_generated_filename(const char *name) {
    static const char *suffixes[]={".dat",".tmp",".bak"};
    size_t n=strlen(name);
    for(unsigned i=0;i<sizeof(suffixes)/sizeof(suffixes[0]);i++){
        size_t sn=strlen(suffixes[i]);
        if(n>sn&&!strcmp(name+n-sn,suffixes[i])&&pref_hex_basename(name,n-sn))return 1;
    }
    return 0;
}

static int pref_read_value_file(const char *path,char **value_out) {
#ifdef ZOMBIE_VITA_BUILD
    SceIoStat st;
    if(sceIoGetstat(path,&st)<0)return 0;
    long long raw=(long long)st.st_size;
    if(raw<0||raw>(1<<20))return 0;
    size_t sz=(size_t)raw;
    char*v=(char*)malloc(sz+1);if(!v)return 0;
    SceUID fd=sceIoOpen(path,SCE_O_RDONLY,0);
    if(fd<0){free(v);return 0;}
    size_t done=0;int ok=1;
    while(done<sz){
        int got=sceIoRead(fd,v+done,sz-done);
        if(got<=0){ok=0;break;}
        done+=(size_t)got;
    }
    if(sceIoClose(fd)<0)ok=0;
    if(!ok||memchr(v,0,sz)){free(v);return 0;}
    v[sz]=0;*value_out=v;return 1;
#else
    FILE*f=fopen(path,"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return 0;}long sz=ftell(f);if(sz<0||sz>(1<<20)||fseek(f,0,SEEK_SET)!=0){fclose(f);return 0;}
    char*v=(char*)malloc((size_t)sz+1);if(!v){fclose(f);return 0;}
    int ok=(sz==0||fread(v,1,(size_t)sz,f)==(size_t)sz)&&!ferror(f);if(fclose(f)!=0)ok=0;
    if(!ok||memchr(v,0,(size_t)sz)){free(v);return 0;}v[sz]=0;*value_out=v;return 1;
#endif
}

#ifdef ZOMBIE_VITA_BUILD
static int pref_io_ensure_worker(void);
#endif

static void pref_load(void) {
    if(pref_loaded)return;
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
    closedir(d);pref_dirty=0;
#ifdef ZOMBIE_VITA_BUILD
    /* Pay thread creation during loading, never on the first gameplay save. */
    (void)pref_io_ensure_worker();
#endif
    SAVE_TRACE("[SAVE] loaded registry directory path=%s keys=%u",PREFS_DIR,pref_count);
}

/* Physical filesystem operations. On Vita these are called by the save worker,
 * never by the game thread unless worker creation fails. */
static int pref_write_key_value_now(const char *key,const char *value) {
    if(!pref_ensure_dir())return 0;
    char path[768],tmp[768],bak[768];
    if(!pref_path_for_key(path,sizeof(path),key,PREFS_EXT)||
       !pref_path_for_key(tmp,sizeof(tmp),key,".tmp")||
       !pref_path_for_key(bak,sizeof(bak),key,".bak"))return 0;
    FILE*f=fopen(tmp,"wb");
    if(!f){SAVE_TRACE("[SAVE] write-one open hash=%08X ok=0 errno=%d",pref_key_hash(key),errno);return 0;}
    size_t n=strlen(value);
    int ok=(n==0||fwrite(value,1,n,f)==n);
    if(ok)ok=fflush(f)==0;
    if(fclose(f)!=0)ok=0;
    if(!ok){remove(tmp);SAVE_TRACE("[SAVE] write-one hash=%08X bytes=%u ok=0 errno=%d",pref_key_hash(key),(unsigned)n,errno);return 0;}
    int had=(access(path,F_OK)==0);
    if(had){remove(bak);if(rename(path,bak)!=0){remove(tmp);SAVE_TRACE("[SAVE] write-one rotate hash=%08X ok=0 errno=%d",pref_key_hash(key),errno);return 0;}}
    if(rename(tmp,path)!=0){int saved_errno=errno;if(had)rename(bak,path);remove(tmp);errno=saved_errno;SAVE_TRACE("[SAVE] write-one rename hash=%08X ok=0 errno=%d",pref_key_hash(key),errno);return 0;}
    if(had)remove(bak);
    SAVE_TRACE("[SAVE] write-one hash=%08X bytes=%u ok=1",pref_key_hash(key),(unsigned)n);
    return 1;
}

static int pref_delete_key_file_now(const char *key) {
    char path[768],tmp[768],bak[768];int ok=1;
    if(!pref_path_for_key(path,sizeof(path),key,PREFS_EXT)||
       !pref_path_for_key(tmp,sizeof(tmp),key,".tmp")||
       !pref_path_for_key(bak,sizeof(bak),key,".bak"))return 0;
    if(remove(path)!=0&&errno!=ENOENT)ok=0;
    if(remove(tmp)!=0&&errno!=ENOENT)ok=0;
    if(remove(bak)!=0&&errno!=ENOENT)ok=0;
    SAVE_TRACE("[SAVE] delete-one hash=%08X ok=%d",pref_key_hash(key),ok);
    return ok;
}

static int pref_clear_files_now(void) {
    if(!pref_ensure_dir())return 0;
    DIR*d=opendir(PREFS_DIR);if(!d)return errno==ENOENT;
    struct dirent*de;int ok=1;unsigned removed=0;
    while((de=readdir(d))!=NULL){
        if(!pref_generated_filename(de->d_name))continue;
        char path[768];int n=snprintf(path,sizeof(path),"%s/%s",PREFS_DIR,de->d_name);
        if(n<=0||(size_t)n>=sizeof(path)){ok=0;continue;}
        if(remove(path)==0)removed++;else if(errno!=ENOENT)ok=0;
    }
    closedir(d);SAVE_TRACE("[SAVE] clear registry files removed=%u ok=%d",removed,ok);return ok;
}

#ifdef ZOMBIE_VITA_BUILD
enum { PREF_IO_WRITE=1, PREF_IO_DELETE=2, PREF_IO_CLEAR=3 };
typedef struct PrefIoJob {
    int op;
    char *key;
    char *value;
    struct PrefIoJob *next;
} PrefIoJob;

static pthread_mutex_t pref_io_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pref_io_cond=PTHREAD_COND_INITIALIZER;
static PrefIoJob *pref_io_head,*pref_io_tail;
static int pref_io_worker_state; /* 0=off, 1=starting, 2=ready, 3=failed */

static void pref_io_job_free(PrefIoJob *j){
    if(!j)return;free(j->key);free(j->value);free(j);
}
static void pref_io_queue_drop_all_locked(void){
    PrefIoJob*j=pref_io_head;while(j){PrefIoJob*n=j->next;pref_io_job_free(j);j=n;}
    pref_io_head=pref_io_tail=NULL;
}
static int pref_io_process(PrefIoJob *j){
    if(j->op==PREF_IO_WRITE)return pref_write_key_value_now(j->key,j->value?j->value:"");
    if(j->op==PREF_IO_DELETE)return pref_delete_key_file_now(j->key);
    if(j->op==PREF_IO_CLEAR)return pref_clear_files_now();
    return 0;
}
static void *pref_io_worker_main(void *unused){
    (void)unused;
    for(;;){
        pthread_mutex_lock(&pref_io_mutex);
        while(!pref_io_head)pthread_cond_wait(&pref_io_cond,&pref_io_mutex);
        pthread_mutex_unlock(&pref_io_mutex);

        /* Small debounce window lets bursts to the same Registry key collapse
         * into one physical write without delaying the game thread. */
        sceKernelDelayThread(15000);

        pthread_mutex_lock(&pref_io_mutex);
        PrefIoJob*j=pref_io_head;
        if(j){
            pref_io_head=j->next;
            if(!pref_io_head)pref_io_tail=NULL;
            j->next=NULL;
        }
        pthread_mutex_unlock(&pref_io_mutex);
        if(!j)continue;

        int ok=pref_io_process(j);
        if(!ok)l_error("[SAVE] async Registry I/O failed op=%d hash=%08X",j->op,j->key?pref_key_hash(j->key):0);
        else SAVE_TRACE("[SAVE] async Registry I/O complete op=%d hash=%08X",j->op,j->key?pref_key_hash(j->key):0);
        pref_io_job_free(j);
    }
    return NULL;
}
static int pref_io_ensure_worker(void){
    int state=__atomic_load_n(&pref_io_worker_state,__ATOMIC_ACQUIRE);
    if(state==2)return 1;
    if(state==3)return 0;
    int expected=0;
    if(__atomic_compare_exchange_n(&pref_io_worker_state,&expected,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
        pthread_t thread;pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr,128*1024);
        int rc=pthread_create(&thread,&attr,pref_io_worker_main,NULL);
        pthread_attr_destroy(&attr);
        if(rc!=0){
            __atomic_store_n(&pref_io_worker_state,3,__ATOMIC_RELEASE);
            l_error("[SAVE] async worker creation failed rc=%d; falling back to synchronous I/O",rc);
            return 0;
        }
        pthread_detach(thread);
        __atomic_store_n(&pref_io_worker_state,2,__ATOMIC_RELEASE);
        SAVE_TRACE("[SAVE] async Registry I/O worker ready");
        return 1;
    }
    do{
        sceKernelDelayThread(100);
        state=__atomic_load_n(&pref_io_worker_state,__ATOMIC_ACQUIRE);
    }while(state==1);
    return state==2;
}
static int pref_io_enqueue_key(int op,const char *key,const char *value){
    if(!key||!pref_io_ensure_worker())return 0;
    PrefIoJob*n=(PrefIoJob*)calloc(1,sizeof(*n));if(!n)return 0;
    n->op=op;n->key=pref_dup(key,strlen(key));
    if(!n->key){pref_io_job_free(n);return 0;}
    if(op==PREF_IO_WRITE){
        n->value=pref_dup(value?value:"",strlen(value?value:""));
        if(!n->value){pref_io_job_free(n);return 0;}
    }

    pthread_mutex_lock(&pref_io_mutex);
    /* Coalesce only after the newest pending CLEAR, preserving ordering. */
    PrefIoJob*match=NULL;
    for(PrefIoJob*j=pref_io_head;j;j=j->next){
        if(j->op==PREF_IO_CLEAR)match=NULL;
        else if(j->key&&!strcmp(j->key,key))match=j;
    }
    if(match){
        free(match->value);match->value=n->value;n->value=NULL;match->op=op;
        pref_io_job_free(n);
    }else{
        if(pref_io_tail)pref_io_tail->next=n;else pref_io_head=n;
        pref_io_tail=n;
    }
    pthread_cond_signal(&pref_io_cond);
    pthread_mutex_unlock(&pref_io_mutex);
    return 1;
}
static int pref_io_enqueue_clear(void){
    if(!pref_io_ensure_worker())return 0;
    PrefIoJob*n=(PrefIoJob*)calloc(1,sizeof(*n));if(!n)return 0;n->op=PREF_IO_CLEAR;
    pthread_mutex_lock(&pref_io_mutex);
    /* A clear supersedes every mutation that has not reached disk yet. New
     * writes queued afterwards remain ordered behind this clear. */
    pref_io_queue_drop_all_locked();
    pref_io_head=pref_io_tail=n;
    pthread_cond_signal(&pref_io_cond);
    pthread_mutex_unlock(&pref_io_mutex);
    return 1;
}
#endif

static int pref_write_entry(const PrefEntry *e) {
#ifdef ZOMBIE_VITA_BUILD
    if(pref_io_enqueue_key(PREF_IO_WRITE,e->key,e->value)){
        SAVE_TRACE("[SAVE] write-one queued hash=%08X bytes=%u",pref_key_hash(e->key),(unsigned)strlen(e->value));
        return 1;
    }
#endif
    return pref_write_key_value_now(e->key,e->value);
}

static int pref_delete_key_file(const char *key) {
#ifdef ZOMBIE_VITA_BUILD
    if(pref_io_enqueue_key(PREF_IO_DELETE,key,NULL)){
        SAVE_TRACE("[SAVE] delete-one queued hash=%08X",pref_key_hash(key));
        return 1;
    }
#endif
    return pref_delete_key_file_now(key);
}

static int pref_clear_files(void) {
#ifdef ZOMBIE_VITA_BUILD
    if(pref_io_enqueue_clear()){
        SAVE_TRACE("[SAVE] clear registry files queued");
        return 1;
    }
#endif
    return pref_clear_files_now();
}

/* Mutations are reflected in RAM immediately. On Vita, successful apply means
 * the physical mutation was accepted by the dedicated I/O worker, not that the
 * game thread waited for the memory card. */
static int pref_save(void) {
    pref_load();
    int ok=!pref_dirty;
    SAVE_TRACE("[SAVE] incremental apply path=%s keys=%u dirty=%d ok=%d",PREFS_DIR,pref_count,pref_dirty,ok);
    return ok;
}

static int pref_persist_current_key(const char *key) {
    int x=pref_find(key);
    if(x<0)return 0;
    int ok=pref_write_entry(&pref_entries[x]);
    if(ok)pref_dirty=0;
    return ok;
}

/* Used by the 3.6.1 Registry::storeEncrypted/loadDecrypted compatibility hooks.
 * The key supplied here is SigmaTeam's own Registry::encryptKey() output, so
 * Registry::contains/remove continue to address exactly the same logical key. */
int zombie_registry_direct_put(const char *key,const char *value) {
    if(!key||!value)return 0;
    pref_lock();pref_load();
    int x=pref_find(key),changed=x<0||strcmp(pref_entries[x].value,value)!=0;
    int ok=1;
    if(changed){ok=pref_set(key,value);if(ok)ok=pref_persist_current_key(key);}
    pref_unlock();
    SAVE_TRACE("[SAVE] direct Registry put hash=%08X bytes=%u changed=%d ok=%d",pref_key_hash(key),(unsigned)strlen(value),changed,ok);
    return ok;
}
char *zombie_registry_direct_get(const char *key) {
    if(!key)return NULL;
    char*out=NULL;
    pref_lock();pref_load();
    int x=pref_find(key);
    if(x>=0)out=pref_dup(pref_entries[x].value,strlen(pref_entries[x].value));
    pref_trace_read(key,x>=0);
    pref_unlock();
    return out;
}
