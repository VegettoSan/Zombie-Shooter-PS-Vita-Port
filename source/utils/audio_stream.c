/* MIT. Mode 0 silent; mode 1 retained crash diagnostic; mode 2 decodes the
 * original M4A/AAC in a worker and mixes bounded PCM into the existing port.
 * BaseStream owns filenames, playing state, fades, next-file and loop semantics.
 * Only MusicPlayer virtual backend methods are replaced, all guarded first. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>
#include "utils/music_decoder.h"

#include "utils/settings.h"


/* ------------------------------------------------------------------------- */
/* Legacy mode 1: keep the exact PCM experiment for A/B crash reproduction. */

static const char *(*string_data)(const void *);
static const char *names[]={"menu_mus01","mus01","mus02","amb01","rain"};
#ifndef ZOMBIE_MUSIC_HOST
#include <sndfile.h>
#include <so_util/so_util.h>
#include <kubridge.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include "utils/logger.h"
extern so_module so_mod;
typedef void (*CreateMusic)(void *, const void *, const void *);
static CreateMusic legacy_original;
static void (*string_init)(void *, const char *);
static void (*string_destroy)(void *);
static unsigned reported, missing;

static void legacy_create_music(void *result,const void *engine,const void *filename) {
    if (!string_data) {
        legacy_original(result,engine,filename);
        return;
    }
    const char *name=string_data(filename);
    if (!name) {
        l_perf("audio_stream legacy null_filename fallback=original");
        legacy_original(result,engine,filename);
        return;
    }
    char expected[64],path[256];
    for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);++i) {
        snprintf(expected,sizeof(expected),"music/%s.m4a",names[i]);
        /* resolveResourcePath may prepend assets/ or the absolute data root. */
        size_t len=strlen(name),tail=strlen(expected);
        if(len<tail || strcmp(name+len-tail,expected) ||
           (len!=tail && name[len-tail-1]!='/')) continue;
        snprintf(path,sizeof(path),DATA_PATH "assets/music/%s.wav",names[i]);
        SF_INFO info={0}; SNDFILE *file=sf_open(path,SFM_READ,&info);
        if(!file) {
            if(!(__atomic_fetch_or(&missing,1u<<i,__ATOMIC_RELAXED)&(1u<<i)))
                l_perf("audio_stream missing_pcm=%s fallback=original",path);
            break;
        }
        int supported=info.frames>0 && info.channels>=1 && info.channels<=2 &&
            (info.format&SF_FORMAT_TYPEMASK)==SF_FORMAT_WAV &&
            ((info.format&SF_FORMAT_SUBMASK)==SF_FORMAT_PCM_16 ||
             (info.format&SF_FORMAT_SUBMASK)==SF_FORMAT_PCM_U8) &&
            (info.samplerate==11025 || info.samplerate==22050 || info.samplerate==44100);
        sf_close(file);
        if(!supported) {
            if(!(__atomic_fetch_or(&missing,1u<<i,__ATOMIC_RELAXED)&(1u<<i)))
                l_perf("audio_stream unsupported_pcm=%s format=0x%X rate=%d channels=%d replace_with_pcm16=1",path,info.format,info.samplerate,info.channels);
            break;
        }
        if(!(__atomic_fetch_or(&reported,1u<<i,__ATOMIC_RELAXED)&(1u<<i)))
            l_perf("audio_stream requested=%s path=%s rate=%d channels=%d frames=%llu format=0x%X backend=OpenSL_URI_PCM",name,path,info.samplerate,info.channels,(unsigned long long)info.frames,info.format);
        uint32_t local_string[3];
        string_init(local_string,path);
        legacy_original(result,engine,local_string);
        static unsigned returned;
        if(!(__atomic_fetch_or(&returned,1u<<i,__ATOMIC_RELAXED)&(1u<<i)))
            l_perf("audio_stream player_object=%08X track=%s",(unsigned)((uintptr_t *)result)[0],names[i]);
        string_destroy(local_string);
        return;
    }
    legacy_original(result,engine,filename);
}

static void install_legacy_pcm_hook(void) {
    uintptr_t addr=(uintptr_t)so_symbol(&so_mod,"_ZNK8opensles6Engine17createMusicPlayerERK6STRING");
    uintptr_t entry=addr&~(uintptr_t)1,arena=(so_mod.patch_head+3)&~(uintptr_t)3;
    const uint32_t prologue[]={0xaf03b5f0,0x0700e92d};
    string_init=(void *)so_symbol(&so_mod,"_ZN6STRINGC2EPKc");
    string_destroy=(void *)so_symbol(&so_mod,"_ZN6STRINGD1Ev");
    string_data=(void *)so_symbol(&so_mod,"_ZNK6STRING5c_strEv");
    if(!string_init || !string_destroy || !string_data || !(addr&1) || entry!=so_mod.load_addr+0x4d20f8 ||
        arena<so_mod.patch_base || arena>so_mod.patch_base+so_mod.patch_size ||
        so_mod.patch_base+so_mod.patch_size-arena<16 || memcmp((void *)entry,prologue,8)) {
        l_perf("audio_stream installed=0 music_mode=1 reason=verified_hook_unavailable");return;
    }
    uint32_t code[]={prologue[0],prologue[1],0xf000f8df,(uint32_t)(entry+8)|1};
    sceClibMemcpy((void *)arena,code,sizeof(code));legacy_original=(CreateMusic)(arena|1);
    so_mod.patch_head=arena+sizeof(code);hook_addr(addr,(uintptr_t)legacy_create_music);
    kuKernelFlushCaches((void *)arena,sizeof(code));kuKernelFlushCaches((void *)entry,8);
    l_perf("audio_stream installed=1 music_mode=1 legacy_pcm_crash_reproducer=1 pcm_sidecars=5");
}



#endif
#define MUSIC_VOICES 4
#define MUSIC_RING_FRAMES 8192u
#define MUSIC_CHUNK 1024u
#define MUSIC_COMPRESSED_BUDGET (8u*1024u*1024u)
typedef struct {
    void *owner;unsigned generation;int claimed,paused,loop,gain,ready,eof,failed;
    unsigned read,write,count;char track[24];
    int16_t pcm[MUSIC_RING_FRAMES*2];
} Voice;
static Voice voices[MUSIC_VOICES];
static pthread_mutex_t music_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_t worker;static int worker_valid,worker_stop;
static unsigned next_generation;
static int music_backend_installed;
static struct { unsigned underruns,lock_misses,decoded_frames,loops,open_errors,decode_errors,decode_us,decode_max,active,compressed_bytes,peak_bytes; } music_stats;
static void (*native_stop)(void *);
static unsigned char *owner_byte(void *self,unsigned offset) { return (unsigned char *)self+offset; }
static int owner_int(void *self,unsigned offset) { int v;memcpy(&v,(char *)self+offset,4);return v; }
static const char *known_track(const char *path) {
    if(!path) return NULL;const char *base=path;
    for(const char *p=path;*p;p++) if(*p=='/' || *p=='\\') base=p+1;
    for(unsigned i=0;i<5;i++) {
        size_t n=strlen(names[i]);
        if(!strncmp(base,names[i],n) && (!strcmp(base+n,".ogg") || !strcmp(base+n,".m4a") || !base[n])) return names[i];
    }
    return NULL;
}
static int find_owner(void *owner) {
    for(int i=0;i<MUSIC_VOICES;i++) if(voices[i].claimed && voices[i].owner==owner) return i;return -1;
}
static int gain_from_percent(int v) { if(v<0) v=0;if(v>100) v=100;return v*32767/100; }
static bool music_open(void *self,const void *filename) {
    const char *track=known_track(string_data(filename));if(!track || !worker_valid) return false;
    pthread_mutex_lock(&music_mutex);int i=find_owner(self);
    if(i<0) for(int j=0;j<MUSIC_VOICES;j++) if(!voices[j].claimed) { i=j;break; }
    if(i>=0) {
        Voice *v=voices+i;memset(v,0,sizeof(*v));v->owner=self;v->claimed=1;
        v->loop=*owner_byte(self,40)!=0;v->gain=gain_from_percent(owner_int(self,32));
        strcpy(v->track,track);v->generation=++next_generation;
        // Original onOpen resets MusicPlayer::setShouldStop flag.
        __atomic_store_n(owner_byte(self,68),0,__ATOMIC_RELEASE);
    }
    pthread_mutex_unlock(&music_mutex);return i>=0;
}
static void music_close(void *self) {
    pthread_mutex_lock(&music_mutex);int i=find_owner(self);
    if(i>=0) { memset(voices+i,0,sizeof(Voice));voices[i].generation=++next_generation; }
    pthread_mutex_unlock(&music_mutex);
}
static void music_pause(void *self) {
    pthread_mutex_lock(&music_mutex);int i=find_owner(self);if(i>=0) voices[i].paused=1;pthread_mutex_unlock(&music_mutex);
}
static void music_resume(void *self) {
    pthread_mutex_lock(&music_mutex);int i=find_owner(self);if(i>=0) voices[i].paused=0;pthread_mutex_unlock(&music_mutex);
}
static void music_volume(void *self,int percent) {
    pthread_mutex_lock(&music_mutex);int i=find_owner(self);if(i>=0) voices[i].gain=gain_from_percent(percent);pthread_mutex_unlock(&music_mutex);
}
static void music_update(void *self) {
    pthread_mutex_lock(&music_mutex);int i=find_owner(self);
    int done=i>=0 && (voices[i].failed || (voices[i].eof && !voices[i].count));
    pthread_mutex_unlock(&music_mutex);
    if(done || __atomic_load_n(owner_byte(self,68),__ATOMIC_ACQUIRE)) native_stop(self);
}
static void *music_worker(void *unused) {
    (void)unused;MusicDecoder *decoder[MUSIC_VOICES]={0};unsigned generation[MUSIC_VOICES]={0};
    int16_t pcm[MUSIC_CHUNK*2];
    while(!__atomic_load_n(&worker_stop,__ATOMIC_ACQUIRE)) {
        int progressed=0;
        for(int i=0;i<MUSIC_VOICES;i++) {
            pthread_mutex_lock(&music_mutex);Voice *v=voices+i;
            unsigned serial=v->generation;int claimed=v->claimed,paused=v->paused,loop=v->loop;
            unsigned count=v->count;int eof=v->eof,failed=v->failed;char track[24];strcpy(track,v->track);
            pthread_mutex_unlock(&music_mutex);
            if(serial!=generation[i]) {
                size_t old_bytes=music_decoder_bytes(decoder[i]);music_decoder_destroy(decoder[i]);decoder[i]=NULL;
                pthread_mutex_lock(&music_mutex);music_stats.compressed_bytes-=(unsigned)old_bytes;pthread_mutex_unlock(&music_mutex);
                generation[i]=serial;
                if(claimed) {
                    char path[256];snprintf(path,sizeof(path),DATA_PATH "assets/music/%s.m4a",track);
                    decoder[i]=music_decoder_open(path);
                    size_t bytes=music_decoder_bytes(decoder[i]);
                    pthread_mutex_lock(&music_mutex);
                    if(bytes+music_stats.compressed_bytes>MUSIC_COMPRESSED_BUDGET) {
                        pthread_mutex_unlock(&music_mutex);music_decoder_destroy(decoder[i]);decoder[i]=NULL;bytes=0;
                        pthread_mutex_lock(&music_mutex);
                    }
                    music_stats.compressed_bytes+=(unsigned)bytes;
                    if(music_stats.compressed_bytes>music_stats.peak_bytes) music_stats.peak_bytes=music_stats.compressed_bytes;
                    if(!decoder[i]) music_stats.open_errors++;
                    if(voices[i].generation==serial) { voices[i].ready=0;voices[i].failed=decoder[i]==NULL; }
                    pthread_mutex_unlock(&music_mutex);
                }
                progressed=1;continue;
            }
            if(!claimed || paused || eof || failed || !decoder[i] || count>MUSIC_RING_FRAMES-MUSIC_CHUNK) continue;
            uint64_t begin=sceKernelGetProcessTimeWide();
            int got=music_decoder_read(decoder[i],pcm,MUSIC_CHUNK,loop);
            unsigned us=(unsigned)(sceKernelGetProcessTimeWide()-begin);
            pthread_mutex_lock(&music_mutex);music_stats.decode_us+=us;if(us>music_stats.decode_max) music_stats.decode_max=us;
            v=voices+i;
            if(v->generation==serial) {
                if(got<0) { v->failed=1;music_stats.decode_errors++; }
                else if(!got) { v->eof=1;if(v->count) v->ready=1; }
                else {
                    for(int f=0;f<got;f++) { v->pcm[v->write*2]=pcm[f*2];v->pcm[v->write*2+1]=pcm[f*2+1];v->write=(v->write+1)%MUSIC_RING_FRAMES; }
                    v->count+=(unsigned)got;if(v->count>=2048) v->ready=1;music_stats.decoded_frames+=(unsigned)got;
                }
            }
            pthread_mutex_unlock(&music_mutex);progressed=1;
        }
        if(!progressed) sceKernelDelayThread(2000);
    }
    for(int i=0;i<MUSIC_VOICES;i++) music_decoder_destroy(decoder[i]);return NULL;
}
void zombie_music_mix(int16_t *samples,unsigned frames) {
    if(setting_music_mode!=2 || !samples || !worker_valid) return;
    if(pthread_mutex_trylock(&music_mutex)) { __atomic_fetch_add(&music_stats.lock_misses,1,__ATOMIC_RELAXED);return; }
    for(int i=0;i<MUSIC_VOICES;i++) {
        Voice *v=voices+i;if(!v->claimed || v->paused || !v->ready || v->failed) continue;
        unsigned n=frames<v->count?frames:v->count;
        if(n<frames && !v->eof) music_stats.underruns++;
        for(unsigned f=0;f<n;f++) {
            for(unsigned c=0;c<2;c++) {
                int mixed=samples[f*2+c]+(v->pcm[v->read*2+c]*v->gain)/32767;
                if(mixed>32767) mixed=32767;if(mixed<-32768) mixed=-32768;samples[f*2+c]=(int16_t)mixed;
            }
            v->read=(v->read+1)%MUSIC_RING_FRAMES;
        }
        v->count-=n;
    }
    pthread_mutex_unlock(&music_mutex);
}
void zombie_music_start(void) {
    if(setting_music_mode!=2 || !music_backend_installed || worker_valid) return;
    worker_stop=0;
    if(!pthread_create(&worker,NULL,music_worker,NULL)) worker_valid=1;
    else l_perf("music worker_start_failed=1");
}
void zombie_music_shutdown(void) {
    if(worker_valid) { __atomic_store_n(&worker_stop,1,__ATOMIC_RELEASE);pthread_join(worker,NULL);worker_valid=0; }
    pthread_mutex_lock(&music_mutex);memset(voices,0,sizeof(voices));music_stats.compressed_bytes=0;pthread_mutex_unlock(&music_mutex);
}
void zombie_music_report(void) {
    static unsigned old_underruns,old_misses,old_decoded,old_open,old_error,old_us;
    pthread_mutex_lock(&music_mutex);
    unsigned active=0,queued=0;for(int i=0;i<MUSIC_VOICES;i++) if(voices[i].claimed) { active++;queued+=voices[i].count; }
    unsigned underruns=music_stats.underruns,misses=__atomic_load_n(&music_stats.lock_misses,__ATOMIC_RELAXED),decoded=music_stats.decoded_frames;
    unsigned opens=music_stats.open_errors,errors=music_stats.decode_errors,us=music_stats.decode_us,max=music_stats.decode_max,bytes=music_stats.compressed_bytes,peak=music_stats.peak_bytes;
    pthread_mutex_unlock(&music_mutex);
    l_perf("music backend=original_m4a_aac worker=%d active=%u queued_frames=%u ring_bytes=%u decoder_accounted_bytes=%u decoder_peak_accounted_bytes=%u decoded_frames=%u underrun_chunks=%u mixer_lock_misses=%u open_errors=%u decode_errors=%u decode_us=%u decode_max_us_lifetime=%u native_codec_heap_not_included=1",
        worker_valid,active,queued,(unsigned)sizeof(voices),bytes,peak,decoded-old_decoded,underruns-old_underruns,misses-old_misses,opens-old_open,errors-old_error,us-old_us,max);
    old_underruns=underruns;old_misses=misses;old_decoded=decoded;old_open=opens;old_error=errors;old_us=us;
}
#ifndef ZOMBIE_MUSIC_HOST
static void install_original_music(void) {
    const struct { const char *name;unsigned offset;uint32_t prologue[2];uintptr_t replacement; } hooks[]={
        {"_ZN5sound11MusicPlayer6onOpenERK6STRING",0x4d4174,{0xaf03b5f0,0xbd04f84d},(uintptr_t)music_open},
        {"_ZN5sound11MusicPlayer7onCloseEv",0x4d46e8,{0xaf03b5f0,0x8d04f84d},(uintptr_t)music_close},
        {"_ZN5sound11MusicPlayer8onUpdateEv",0x4d485c,{0xaf02b5b0,0x4941b0a6},(uintptr_t)music_update},
        {"_ZN5sound11MusicPlayer7onPauseEv",0x4d4990,{0xaf03b5f0,0xbd04f84d},(uintptr_t)music_pause},
        {"_ZN5sound11MusicPlayer8onResumeEv",0x4d4ad0,{0xaf02b5b0,0x0438f100},(uintptr_t)music_resume},
        {"_ZN5sound11MusicPlayer12updateVolumeEi",0x4d4058,{0xaf02b5b0,0x4a3bb0a6},(uintptr_t)music_volume}
    };
    uintptr_t addresses[6];
    string_data=(void *)so_symbol(&so_mod,"_ZNK6STRING5c_strEv");
    native_stop=(void *)so_symbol(&so_mod,"_ZN5sound10BaseStream4stopEv");
    const uint32_t string_prologue=0x47706800u;
    const uint32_t stop_prologue[]={0xaf02b5b0u,0x4604b0a4u};
    if((uintptr_t)string_data!=so_mod.load_addr+0x3dc789u ||
       (uintptr_t)native_stop!=so_mod.load_addr+0x4d4da9u ||
       memcmp((void *)((uintptr_t)string_data&~1u),&string_prologue,4) ||
       memcmp((void *)((uintptr_t)native_stop&~1u),stop_prologue,8)) {
        l_perf("audio_stream installed=0 music_mode=2 guard_failed=backend_helpers");return;
    }
    for(unsigned i=0;i<6;i++) {
        addresses[i]=(uintptr_t)so_symbol(&so_mod,hooks[i].name);
        if(addresses[i]!=so_mod.load_addr+hooks[i].offset+1 || memcmp((void *)(addresses[i]&~1u),hooks[i].prologue,8)) {
            l_perf("audio_stream installed=0 music_mode=2 guard_failed=%s",hooks[i].name);return;
        }
    }
    for(unsigned i=0;i<6;i++) { hook_addr(addresses[i],hooks[i].replacement);kuKernelFlushCaches((void *)(addresses[i]&~1u),8); }
    music_backend_installed=1;
    l_perf("audio_stream installed=1 music_mode=2 backend=original_m4a_aac_single_mixer guarded_methods=6 voices=4 ring_frames_per_voice=8192 compressed_budget_mib=8");
}
void audio_stream_install(void) {
    if(setting_music_mode==2) { install_original_music();return; }
    if(setting_music_mode==1) { install_legacy_pcm_hook();return; }
    l_perf("audio_stream installed=0 music_mode=0 backend=stable_silent_fallback music_hook_skipped=1");
}

#endif
