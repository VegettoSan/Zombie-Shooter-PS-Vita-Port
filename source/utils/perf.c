/* Low-frequency waits are timed; high-frequency small asset reads are sampled.
 * Counters are cumulative; snapshots never reset a concurrent writer's data. */
#include "utils/perf.h"
#include "utils/logger.h"
#include "utils/asset_index.h"
#include "utils/asset_cache.h"
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/unistd.h>
#ifdef USE_SCELIBC_IO
#include <libc_bridge/libc_bridge.h>
#endif
#ifdef NDK_PORT
#include <falso_ndk/FalsoNDK.h>
#endif
static AudioPerfStats audio;
#define ADD(p,v) __atomic_fetch_add((p),(v),__ATOMIC_RELAXED)
#define LOAD(p) __atomic_load_n((p),__ATOMIC_RELAXED)
static void max_relaxed(unsigned *p, unsigned v) {
    unsigned old = LOAD(p);
    while (v > old && !__atomic_compare_exchange_n(p, &old, v, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
}
void audio_perf_call(unsigned kind) { ADD(kind ? &audio.destroy.calls : &audio.clear.calls, 1); }
void audio_perf_wait(unsigned kind, unsigned us, int timeout) {
    AudioWaitStats *s = kind ? &audio.destroy : &audio.clear;
    ADD(&s->wait_calls, 1); ADD(&s->wait_us, us); max_relaxed(&s->max_us, us);
    if (timeout) ADD(&s->timeouts, 1);
}
void audio_perf_output(int result) { ADD(&audio.output_calls, 1); if (result < 0) ADD(&audio.output_errors, 1); }
static struct { unsigned frames,rate,fill_us,fill_max,gap_max,late,deadline_misses,silence,enqueues,enqueue_errors,source_bytes,converted_bytes,allocations,depth_max; } at;
void audio_perf_enqueue(unsigned bytes,unsigned converted_bytes,unsigned depth,int result) {
    ADD(&at.enqueues,1);if(result) ADD(&at.enqueue_errors,1);
    ADD(&at.source_bytes,bytes);ADD(&at.converted_bytes,converted_bytes);
    if(converted_bytes) ADD(&at.allocations,1);max_relaxed(&at.depth_max,depth);
}
void audio_perf_output_timing(int result,uint64_t begin,uint64_t submit,unsigned frames,unsigned rate,const void *pcm) {
    static uint64_t last_submit;
    unsigned fill=(unsigned)(submit-begin),expected=(unsigned)((uint64_t)frames*1000000/rate);
    ADD(&at.fill_us,fill);max_relaxed(&at.fill_max,fill);
    __atomic_store_n(&at.frames,frames,__ATOMIC_RELAXED);__atomic_store_n(&at.rate,rate,__ATOMIC_RELAXED);
    if(last_submit) { unsigned gap=(unsigned)(submit-last_submit);max_relaxed(&at.gap_max,gap);
        if(gap>expected+2000) ADD(&at.late,1);if(gap>expected*2) ADD(&at.deadline_misses,1); }
    last_submit=submit;
    const uint32_t *words=pcm;unsigned nonzero=0;
    for(unsigned i=0;i<frames;i++) if(words[i]) { nonzero=1;break; }
    if(!nonzero) ADD(&at.silence,1);
    audio_perf_output(result);
}
void audio_timing_report(void) {
    static unsigned old_fill,old_late,old_miss,old_silence,old_enqueue,old_errors,old_bytes,old_converted,old_alloc;
    unsigned fill=LOAD(&at.fill_us),late=LOAD(&at.late),miss=LOAD(&at.deadline_misses),silence=LOAD(&at.silence);
    unsigned enq=LOAD(&at.enqueues),err=LOAD(&at.enqueue_errors),bytes=LOAD(&at.source_bytes),converted=LOAD(&at.converted_bytes),alloc=LOAD(&at.allocations);
    unsigned frames=LOAD(&at.frames),rate=LOAD(&at.rate);
    l_perf("audio_timing frames=%u rate=%u expected_us=%u fill_total_us=%u fill_max_us_lifetime=%u max_output_gap_us_lifetime=%u late_wakeups=%u deadline_misses=%u silence_buffers=%u enqueues=%u enqueue_errors=%u source_bytes=%u converted_bytes=%u enqueue_allocations=%u queue_depth_max_lifetime=%u silence_is_not_underrun=1",
        frames,rate,rate?(unsigned)((uint64_t)frames*1000000/rate):0,fill-old_fill,LOAD(&at.fill_max),LOAD(&at.gap_max),late-old_late,miss-old_miss,silence-old_silence,
        enq-old_enqueue,err-old_errors,bytes-old_bytes,converted-old_converted,alloc-old_alloc,LOAD(&at.depth_max));
    old_fill=fill;old_late=late;old_miss=miss;old_silence=silence;old_enqueue=enq;old_errors=err;old_bytes=bytes;old_converted=converted;old_alloc=alloc;
}
void audio_perf_snapshot(AudioPerfStats *out) {
#define COPY(member) out->member = LOAD(&audio.member)
    COPY(clear.calls); COPY(clear.wait_calls); COPY(clear.wait_us); COPY(clear.max_us); COPY(clear.timeouts);
    COPY(destroy.calls); COPY(destroy.wait_calls); COPY(destroy.wait_us); COPY(destroy.max_us); COPY(destroy.timeouts);
    COPY(output_calls); COPY(output_errors);
#undef COPY
}
/* Each registered thread owns its counters. Relaxed load/store publication
 * costs no atomic RMW on asset reads; only first registration uses CAS. */
typedef struct { unsigned calls, bytes, samples, us, max_us, errors; } IOStats;
typedef struct { unsigned calls, us, max_us, positive, infinite, max_timeout; } WaitStats;
static struct { int owner; IOStats asset, file, read, asset_open, asset_seek, asset_open_ok, asset_open_fail, bulk_fill, engine[PERF_ENGINE_COUNT]; WaitStats once, all; } slots[16];
static unsigned dropped_threads;
static int slot_index(void) {
    int tid = sceKernelGetThreadId();
    for (int i=0; i<16; ++i) if (LOAD(&slots[i].owner) == tid) return i;
    for (int i=0; i<16; ++i) {
        int zero=0;
        if (__atomic_compare_exchange_n(&slots[i].owner, &zero, tid, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) return i;
    }
    ADD(&dropped_threads,1); return -1;
}
#define STORE(p,v) __atomic_store_n((p),(v),__ATOMIC_RELAXED)
#define INC(p,v) STORE((p),LOAD(p)+(unsigned)(v))
static void io_record(IOStats *s, int bytes, uint64_t start) {
    if (!s) return;
    INC(&s->calls,1);
    if (bytes < 0) INC(&s->errors,1); else INC(&s->bytes,bytes);
    if (start) {
        unsigned us=(unsigned)(sceKernelGetProcessTimeWide()-start);
        INC(&s->samples,1); INC(&s->us,us);
        if (us>LOAD(&s->max_us)) STORE(&s->max_us,us);
    }
}
void perf_bulk_memset(size_t bytes,uint64_t start) {
    int i=slot_index();io_record(i<0?NULL:&slots[i].bulk_fill,(int)bytes,start);
}
void perf_engine_phase(unsigned phase,uint64_t start) {
    if(phase>=PERF_ENGINE_COUNT) return;
    int i=slot_index();io_record(i<0?NULL:&slots[i].engine[phase],0,start);
}
#ifdef NDK_PORT
/* At most one slow success/failure path per 5-second report. The short lock
 * protects text snapshots only; it is never held across asset I/O or logging. */
typedef struct { unsigned us; uintptr_t caller; int mode, truncated; char name[192]; } AssetOpenWorst;
static AssetOpenWorst open_worst[2];
static int open_worst_lock;
static void lock_open_worst(void) {
    while (__atomic_exchange_n(&open_worst_lock,1,__ATOMIC_ACQUIRE)) {}
}
static void unlock_open_worst(void) { __atomic_store_n(&open_worst_lock,0,__ATOMIC_RELEASE); }
static void record_slow_open(const char *name,int mode,uintptr_t caller,int ok,unsigned us) {
    if (us < 10000) return;
    lock_open_worst();
    AssetOpenWorst *w=&open_worst[ok?0:1];
    if(us>w->us) {
        w->us=us;w->caller=caller;w->mode=mode;
        if(!name) name="<null>";
        unsigned n=0;
        for(;n<sizeof(w->name)-1 && name[n];++n) {
            unsigned char c=(unsigned char)name[n];
            w->name[n]=(c<32 || c=='"')?'?':(char)c;
        }
        w->name[n]=0;w->truncated=name[n]!=0;
    }
    unlock_open_worst();
}
static void report_slow_opens(void) {
    AssetOpenWorst snapshot[2];
    lock_open_worst();memcpy(snapshot,open_worst,sizeof(snapshot));
    memset(open_worst,0,sizeof(open_worst));unlock_open_worst();
    for(unsigned i=0;i<2;++i) if(snapshot[i].us)
        l_perf("asset_open_slow result=%s us=%u caller=0x%08X mode=%d truncated=%d name=\"%s\"",
            i?"failed":"ok",snapshot[i].us,(unsigned)snapshot[i].caller,
            snapshot[i].mode,snapshot[i].truncated,snapshot[i].name);
}
AAsset *AAssetManager_open_perf(AAssetManager *mgr, const char *name, int mode) {
    int i=slot_index(); IOStats *s=i<0?NULL:&slots[i].asset_open;
    uint64_t start=sceKernelGetProcessTimeWide();
    char normalized[256];
    if(name && strlen(name)<sizeof(normalized)) {
        strcpy(normalized,name);for(char *p=normalized;*p;p++) if(*p=='\\') *p='/';name=normalized;
    }
    AAsset *ret=asset_index_missing(name)?NULL:AAssetManager_open(mgr,name,mode);
    int saved_errno=errno;
    unsigned us=(unsigned)(sceKernelGetProcessTimeWide()-start);
    io_record(s,ret?0:-1,start);
    IOStats *detail=i<0?NULL:(ret?&slots[i].asset_open_ok:&slots[i].asset_open_fail);
    io_record(detail,ret?0:-1,start);
    record_slow_open(name,mode,(uintptr_t)__builtin_return_address(0),ret!=NULL,us);
    errno=saved_errno;
    return ret;
}
off_t AAsset_seek_perf(AAsset *asset, off_t offset, int whence) {
    int i=slot_index(); IOStats *s=i<0?NULL:&slots[i].asset_seek;
    uint64_t start=sceKernelGetProcessTimeWide();
    off_t ret=AAsset_seek(asset,offset,whence);
    io_record(s,ret<0?-1:0,start); return ret;
}
int AAsset_read_perf(AAsset *asset, void *buf, size_t count) {
    int i=slot_index(); IOStats *s=i<0?NULL:&slots[i].asset;
    uint64_t start=s && (count>=4096 || (LOAD(&s->calls)&255)==0) ? sceKernelGetProcessTimeWide():0;
    int n=AAsset_read(asset,buf,count); io_record(s,n,start); return n;
}
static int poll_perf(int all, int timeout, int *fd, int *events, void **data) {
    int i=slot_index(); WaitStats *s=i<0?NULL:(all?&slots[i].all:&slots[i].once);
    uint64_t start=sceKernelGetProcessTimeWide();
    int ret=all?ALooper_pollAll(timeout,fd,events,data):ALooper_pollOnce(timeout,fd,events,data);
    unsigned us=(unsigned)(sceKernelGetProcessTimeWide()-start);
    if (s) {
        INC(&s->calls,1); INC(&s->us,us);
        if(us>LOAD(&s->max_us)) STORE(&s->max_us,us);
        if(timeout>0) { INC(&s->positive,1); if((unsigned)timeout>LOAD(&s->max_timeout)) STORE(&s->max_timeout,timeout); }
        if(timeout<0) INC(&s->infinite,1);
    }
    return ret;
}
int ALooper_pollOnce_perf(int timeout,int *fd,int *events,void **data) { return poll_perf(0,timeout,fd,events,data); }
int ALooper_pollAll_perf(int timeout,int *fd,int *events,void **data) { return poll_perf(1,timeout,fd,events,data); }
#endif
size_t fread_perf(void *ptr,size_t size,size_t n,FILE *stream) {
    int i=slot_index(); IOStats *s=i<0?NULL:&slots[i].file;
    uint64_t start=sceKernelGetProcessTimeWide();
#ifdef USE_SCELIBC_IO
    size_t ret=sceLibcBridge_fread(ptr,size,n,stream);
#else
    size_t ret=fread(ptr,size,n,stream);
#endif
    io_record(s,(int)(ret*size),start); return ret;
}
ssize_t read_perf(int fd,void *buf,size_t count) {
    int i=slot_index(); IOStats *s=i<0?NULL:&slots[i].read;
    uint64_t start=sceKernelGetProcessTimeWide();
#ifdef NDK_PORT
    ssize_t ret=fndk_read(fd,buf,count);
#else
    ssize_t ret=read(fd,buf,count);
#endif
    io_record(s,(int)ret,start); return ret;
}
void raster_palette_report(void);
void raster_alpha_report(void);
void render_reuse_report(void);
void perf_report(void) {
    raster_palette_report();
    raster_alpha_report();
    render_reuse_report();
    audio_timing_report();
    extern void map_profile_report(void);
    map_profile_report();
    extern void zombie_music_report(void);
    zombie_music_report();
    static AudioPerfStats previous;
    static LoggerStats old_log;
    AudioPerfStats now; audio_perf_snapshot(&now);
#define D(m) (now.m-previous.m)
    l_perf("audio clear_calls=%u clear_wait_calls=%u clear_wait_us=%u clear_max_us_lifetime=%u clear_timeouts=%u destroy_calls=%u destroy_wait_calls=%u destroy_wait_us=%u destroy_max_us_lifetime=%u destroy_timeouts=%u output_calls=%u output_errors=%u",
        D(clear.calls),D(clear.wait_calls),D(clear.wait_us),now.clear.max_us,D(clear.timeouts),
        D(destroy.calls),D(destroy.wait_calls),D(destroy.wait_us),now.destroy.max_us,D(destroy.timeouts),D(output_calls),D(output_errors));
#undef D
    previous=now;
    IOStats io[8]={{0}}; WaitStats waits[2]={{0}};
    static IOStats old_io[8]; static WaitStats old_waits[2];
    for(int i=0;i<16;++i) {
        if(!LOAD(&slots[i].owner)) continue;
        IOStats *sources[]={&slots[i].asset,&slots[i].file,&slots[i].read,&slots[i].asset_open,&slots[i].asset_seek,&slots[i].asset_open_ok,&slots[i].asset_open_fail,&slots[i].bulk_fill};
        WaitStats *ws[]={&slots[i].once,&slots[i].all};
        for(int j=0;j<8;++j) {
#define SUM_IO(m) io[j].m+=LOAD(&sources[j]->m)
            SUM_IO(calls); SUM_IO(bytes); SUM_IO(samples); SUM_IO(us); SUM_IO(errors);
#undef SUM_IO
            unsigned max=LOAD(&sources[j]->max_us); if(max>io[j].max_us) io[j].max_us=max;
        }
        for(int j=0;j<2;++j) {
#define SUM_WAIT(m) waits[j].m+=LOAD(&ws[j]->m)
            SUM_WAIT(calls); SUM_WAIT(us); SUM_WAIT(positive); SUM_WAIT(infinite);
#undef SUM_WAIT
            unsigned max=LOAD(&ws[j]->max_us); if(max>waits[j].max_us) waits[j].max_us=max;
            max=LOAD(&ws[j]->max_timeout); if(max>waits[j].max_timeout) waits[j].max_timeout=max;
        }
    }
    static IOStats old_engine[PERF_ENGINE_COUNT];
    const char *phase_names[]={"graph", "software", "map", "pre", "post", "collector"};
    for(unsigned j=0;j<PERF_ENGINE_COUNT;++j) {
        IOStats total={0};
        for(int i=0;i<16;++i) if(LOAD(&slots[i].owner)) {
            IOStats *src=&slots[i].engine[j];
            total.calls+=LOAD(&src->calls);total.us+=LOAD(&src->us);
            unsigned max=LOAD(&src->max_us);if(max>total.max_us) total.max_us=max;
        }
        l_perf("engine phase=%s calls=%u total_us=%u max_us_lifetime=%u inclusive=1",phase_names[j],total.calls-old_engine[j].calls,total.us-old_engine[j].us,total.max_us);
        old_engine[j]=total;
    }
    const char *names[]={"asset_sampled","fread","read","asset_open","asset_seek","asset_open_ok","asset_open_fail","bulk_fill"};
    for(int j=0;j<8;++j) {
        l_perf("io kind=%s calls=%u bytes=%u timed_calls=%u measured_us=%u max_us_lifetime=%u errors=%u dropped_thread_calls=%u", names[j],io[j].calls-old_io[j].calls,io[j].bytes-old_io[j].bytes,io[j].samples-old_io[j].samples,io[j].us-old_io[j].us,io[j].max_us,io[j].errors-old_io[j].errors,LOAD(&dropped_threads));
        old_io[j]=io[j];
    }
    for(int j=0;j<2;++j) {
        l_perf("waits kind=poll%s calls=%u total_us=%u max_us_lifetime=%u positive_timeout_calls=%u infinite_timeout_calls=%u requested_max_ms_lifetime=%u",j?"All":"Once",waits[j].calls-old_waits[j].calls,waits[j].us-old_waits[j].us,waits[j].max_us,waits[j].positive-old_waits[j].positive,waits[j].infinite-old_waits[j].infinite,waits[j].max_timeout);
        old_waits[j]=waits[j];
    }
#ifdef NDK_PORT
    report_slow_opens();
    asset_index_report();
    static AssetCacheStats old_cache;AssetCacheStats cache;asset_cache_snapshot(&cache);
    l_perf("asset_cache hits=%u misses=%u evictions=%u avoided_opens=%u saved_read_bytes=%llu bytes=%u peak_bytes=%u limit_bytes=%u entries=%u pinned=%u fallback=%u",
        cache.hits-old_cache.hits,cache.misses-old_cache.misses,cache.evictions-old_cache.evictions,
        cache.avoided_opens-old_cache.avoided_opens,(unsigned long long)(cache.saved_read_bytes-old_cache.saved_read_bytes),
        (unsigned)cache.bytes,(unsigned)cache.peak_bytes,(unsigned)cache.limit,cache.entries,cache.pinned,cache.fallback-old_cache.fallback);
    old_cache=cache;
    AssetPathStats paths[5];unsigned overflow=0,n=asset_cache_paths(paths,&overflow);
    for(unsigned p=0;p<n;p++) l_perf("asset_path name=\"%s\" opens=%u hits=%u closes=%u size=%u closed_handle_read_bytes=%llu path_table_overflow=%u closed_reads_may_cross_window=1",
        paths[p].key,paths[p].opens,paths[p].hits,paths[p].closes,(unsigned)paths[p].size,(unsigned long long)paths[p].read_bytes,overflow);
#endif
    LoggerStats log={0}; logger_get_stats(&log);
    l_perf("logger lines=%u syncs=%u sync_total_us=%u suppressed_repeats=%u total_us=%u",log.lines-old_log.lines,log.syncs-old_log.syncs,log.sync_us-old_log.sync_us,log.suppressed-old_log.suppressed,log.total_us-old_log.total_us);
    old_log=log;
}
