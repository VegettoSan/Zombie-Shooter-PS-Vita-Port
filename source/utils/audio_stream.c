/* MIT. Zombie Shooter music experiments.
 *
 * music_mode 0: stable fallback; do not patch music creation.
 * music_mode 1: legacy PCM16/WAV createMusicPlayer hook retained only to
 *               reproduce the confirmed OpenSL crash.
 * music_mode 2: MetalSyntax-style compressed music path. Intercept the
 *               engine's BaseStream command before OpenSL, keep only the
 *               active OGG/Vorbis files compressed in RAM, decode small PCM
 *               grains with libvorbisfile, and mix them into the existing
 *               Vita OpenSL/sceAudioOut output buffer.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>
#include <sndfile.h>
#include <vorbis/vorbisfile.h>
#include <so_util/so_util.h>
#include <kubridge.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>

#include "utils/logger.h"
#include "utils/settings.h"

extern so_module so_mod;

/* ------------------------------------------------------------------------- */
/* Legacy mode 1: keep the exact PCM experiment for A/B crash reproduction. */

typedef void (*CreateMusic)(void *, const void *, const void *);
static CreateMusic legacy_original;
static void (*string_init)(void *, const char *);
static void (*string_destroy)(void *);
static const char *(*string_data)(const void *);
static const char *names[]={"menu_mus01","mus01","mus02","amb01","rain"};
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

/* ------------------------------------------------------------------------- */
/* Mode 2: compressed OGG decoder mixed into the existing Vita output. */

#define MUSIC_VOICE_COUNT 4
#define MUSIC_DECODE_FRAMES 256
#define MUSIC_FILE_LIMIT (16u * 1024u * 1024u)

typedef struct {
    uint8_t *bytes;
    size_t size;
    size_t pos;
    OggVorbis_File vf;
    int opened;
    int channels;
    long rate;
    char track[24];
} MusicDecoder;

typedef struct {
    void *owner;
    MusicDecoder *decoder;
    unsigned serial;
    int claimed;
    int paused;
    int loop;
    int gain_q15;
} MusicVoice;

static MusicVoice music_voices[MUSIC_VOICE_COUNT];
static pthread_mutex_t music_mutex=PTHREAD_MUTEX_INITIALIZER;
static unsigned music_serial;
static int16_t music_decode_buffer[MUSIC_DECODE_FRAMES*2];

static so_hook base_play_hook;
static so_hook base_stop_hook;
static so_hook base_pause_hook;
static so_hook base_resume_hook;
static so_hook base_volume_hook;

static size_t ogg_mem_read(void *ptr,size_t size,size_t nmemb,void *datasource) {
    MusicDecoder *d=(MusicDecoder *)datasource;
    if(!d || !ptr || !size || !nmemb) return 0;
    if(nmemb>SIZE_MAX/size) return 0;
    size_t request=size*nmemb;
    size_t available=d->pos<d->size ? d->size-d->pos : 0;
    size_t take=request<available?request:available;
    take-=take%size;
    if(take) memcpy(ptr,d->bytes+d->pos,take);
    d->pos+=take;
    return take/size;
}

static int ogg_mem_seek(void *datasource,ogg_int64_t offset,int whence) {
    MusicDecoder *d=(MusicDecoder *)datasource;
    if(!d) return -1;
    ogg_int64_t base;
    if(whence==SEEK_SET) base=0;
    else if(whence==SEEK_CUR) base=(ogg_int64_t)d->pos;
    else if(whence==SEEK_END) base=(ogg_int64_t)d->size;
    else return -1;
    ogg_int64_t next=base+offset;
    if(next<0 || (uint64_t)next>d->size) return -1;
    d->pos=(size_t)next;
    return 0;
}

static long ogg_mem_tell(void *datasource) {
    MusicDecoder *d=(MusicDecoder *)datasource;
    if(!d || d->pos>(size_t)LONG_MAX) return -1;
    return (long)d->pos;
}

static int ogg_mem_close(void *datasource) {
    (void)datasource;
    return 0;
}

static const ov_callbacks ogg_memory_callbacks={
    ogg_mem_read,ogg_mem_seek,ogg_mem_close,ogg_mem_tell
};

static void decoder_destroy(MusicDecoder *d) {
    if(!d) return;
    if(d->opened) ov_clear(&d->vf);
    free(d->bytes);
    free(d);
}

static MusicDecoder *decoder_open(const char *track) {
    char path[256];
    snprintf(path,sizeof(path),DATA_PATH "assets/music/%s.ogg",track);
    SceUID fd=sceIoOpen(path,SCE_O_RDONLY,0);
    if(fd<0) {
        l_perf("music_ogg open_failed track=%s path=%s result=0x%08X",track,path,(unsigned)fd);
        return NULL;
    }
    SceOff end=sceIoLseek(fd,0,SEEK_END);
    if(end<=0 || (uint64_t)end>MUSIC_FILE_LIMIT || sceIoLseek(fd,0,SEEK_SET)<0) {
        l_perf("music_ogg invalid_size track=%s bytes=%lld",track,(long long)end);
        sceIoClose(fd);
        return NULL;
    }
    MusicDecoder *d=(MusicDecoder *)calloc(1,sizeof(*d));
    if(!d) { sceIoClose(fd); return NULL; }
    d->size=(size_t)end;
    d->bytes=(uint8_t *)malloc(d->size);
    if(!d->bytes) { sceIoClose(fd); decoder_destroy(d); return NULL; }
    size_t done=0;
    while(done<d->size) {
        int got=sceIoRead(fd,d->bytes+done,(SceSize)(d->size-done));
        if(got<=0) break;
        done+=(size_t)got;
    }
    sceIoClose(fd);
    if(done!=d->size) {
        l_perf("music_ogg read_failed track=%s read=%u expected=%u",track,(unsigned)done,(unsigned)d->size);
        decoder_destroy(d);return NULL;
    }
    strncpy(d->track,track,sizeof(d->track)-1);
    if(ov_open_callbacks(d,&d->vf,NULL,0,ogg_memory_callbacks)<0) {
        l_perf("music_ogg decoder_open_failed track=%s",track);
        decoder_destroy(d);return NULL;
    }
    d->opened=1;
    vorbis_info *info=ov_info(&d->vf,-1);
    if(!info || (info->channels!=1 && info->channels!=2) || info->rate!=44100) {
        l_perf("music_ogg unsupported track=%s rate=%ld channels=%d expected=44100_stereo_or_mono",
               track,info?info->rate:0,info?info->channels:0);
        decoder_destroy(d);return NULL;
    }
    d->channels=info->channels;
    d->rate=info->rate;
    return d;
}

static const char *known_track(const char *path) {
    if(!path) return NULL;
    const char *base=path;
    for(const char *p=path;*p;++p) if(*p=='/' || *p=='\\') base=p+1;
    size_t base_len=strlen(base);
    for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);++i) {
        size_t n=strlen(names[i]);
        if(base_len<n || strncmp(base,names[i],n)) continue;
        const char *tail=base+n;
        if(*tail==0 || !strcmp(tail,".ogg") || !strcmp(tail,".m4a")) return names[i];
    }
    return NULL;
}

static int voice_index_locked(void *owner) {
    for(int i=0;i<MUSIC_VOICE_COUNT;++i)
        if(music_voices[i].claimed && music_voices[i].owner==owner) return i;
    return -1;
}

static int owner_claimed(void *owner) {
    int found;
    pthread_mutex_lock(&music_mutex);
    found=voice_index_locked(owner)>=0;
    pthread_mutex_unlock(&music_mutex);
    return found;
}

static void voice_release_locked(MusicVoice *v) {
    decoder_destroy(v->decoder);
    memset(v,0,sizeof(*v));
}

static void music_claim_and_play(void *owner,const char *track,int loop) {
    MusicDecoder *fresh=decoder_open(track); /* disk I/O stays off audio mutex */
    pthread_mutex_lock(&music_mutex);
    int slot=voice_index_locked(owner);
    if(slot<0) {
        for(int i=0;i<MUSIC_VOICE_COUNT;++i) if(!music_voices[i].claimed) { slot=i; break; }
    }
    if(slot<0) {
        slot=0;
        for(int i=1;i<MUSIC_VOICE_COUNT;++i)
            if(music_voices[i].serial<music_voices[slot].serial) slot=i;
    }
    voice_release_locked(&music_voices[slot]);
    MusicVoice *v=&music_voices[slot];
    v->owner=owner;
    v->decoder=fresh;
    v->claimed=1;
    v->paused=0;
    v->loop=loop;
    v->gain_q15=32767;
    v->serial=++music_serial;
    pthread_mutex_unlock(&music_mutex);
    if(fresh)
        l_perf("music_ogg play track=%s compressed_bytes=%u rate=%ld channels=%d slot=%d loop=%d",
               track,(unsigned)fresh->size,fresh->rate,fresh->channels,slot,loop);
    else
        l_perf("music_ogg silent_claim track=%s slot=%d reason=missing_or_invalid_sidecar",track,slot);
}

static void music_stop_owner(void *owner) {
    pthread_mutex_lock(&music_mutex);
    int slot=voice_index_locked(owner);
    if(slot>=0) voice_release_locked(&music_voices[slot]);
    pthread_mutex_unlock(&music_mutex);
}

static void music_pause_owner(void *owner,int paused) {
    pthread_mutex_lock(&music_mutex);
    int slot=voice_index_locked(owner);
    if(slot>=0) music_voices[slot].paused=paused;
    pthread_mutex_unlock(&music_mutex);
}

static void music_volume_owner(void *owner,float value) {
    if(value<0.f) value=0.f;
    if(value>1.f) value=1.f;
    pthread_mutex_lock(&music_mutex);
    int slot=voice_index_locked(owner);
    if(slot>=0) music_voices[slot].gain_q15=(int)(value*32767.f+0.5f);
    pthread_mutex_unlock(&music_mutex);
}

static int decoder_read_frames(MusicDecoder *d,int16_t *dst,int max_frames,int loop) {
    int total=0,guard=0;
    while(total<max_frames && guard<8) {
        int bitstream=0;
        int bytes_per_frame=d->channels*2;
        long got=ov_read(&d->vf,(char *)(dst+total*d->channels),
                         (max_frames-total)*bytes_per_frame,0,2,1,&bitstream);
        if(got>0) {
            int frames=(int)(got/bytes_per_frame);
            if(frames<=0) break;
            total+=frames;
            guard=0;
            continue;
        }
        if(got==0) {
            if(loop && ov_pcm_seek(&d->vf,0)==0) { ++guard; continue; }
            break;
        }
        /* OV_HOLE and other recoverable packet errors: bounded retry. */
        ++guard;
    }
    return total;
}

void zombie_music_mix(int16_t *samples,unsigned frames) {
    if(setting_music_mode!=2 || !samples || !frames) return;
    pthread_mutex_lock(&music_mutex);
    for(int vi=0;vi<MUSIC_VOICE_COUNT;++vi) {
        MusicVoice *v=&music_voices[vi];
        if(!v->claimed || v->paused || !v->decoder) continue;
        unsigned cursor=0;
        while(cursor<frames) {
            unsigned chunk=frames-cursor;
            if(chunk>MUSIC_DECODE_FRAMES) chunk=MUSIC_DECODE_FRAMES;
            int got=decoder_read_frames(v->decoder,music_decode_buffer,(int)chunk,v->loop);
            if(got<=0) {
                if(!v->loop) voice_release_locked(v);
                break;
            }
            for(int f=0;f<got;++f) {
                int16_t left,right;
                if(v->decoder->channels==2) {
                    left=music_decode_buffer[f*2];right=music_decode_buffer[f*2+1];
                } else {
                    left=right=music_decode_buffer[f];
                }
                int scaled_l=(left*v->gain_q15)>>15;
                int scaled_r=(right*v->gain_q15)>>15;
                unsigned out=(cursor+(unsigned)f)*2;
                int mix_l=(int)samples[out]+scaled_l;
                int mix_r=(int)samples[out+1]+scaled_r;
                if(mix_l>32767) mix_l=32767; else if(mix_l<-32768) mix_l=-32768;
                if(mix_r>32767) mix_r=32767; else if(mix_r<-32768) mix_r=-32768;
                samples[out]=(int16_t)mix_l;samples[out+1]=(int16_t)mix_r;
            }
            cursor+=(unsigned)got;
            if((unsigned)got<chunk) break;
        }
    }
    pthread_mutex_unlock(&music_mutex);
}

void zombie_music_shutdown(void) {
    pthread_mutex_lock(&music_mutex);
    for(int i=0;i<MUSIC_VOICE_COUNT;++i) voice_release_locked(&music_voices[i]);
    pthread_mutex_unlock(&music_mutex);
}

/* hook_addr()/SO_CONTINUE's generic helper is intentionally not used for
 * these methods. Typed calls preserve the exact C++ softfp argument ABI,
 * especially setVolume(float). */
static void restore_hook(const so_hook *h) {
    sceClibMemcpy((void *)h->addr,h->orig_instr,sizeof(h->orig_instr));
    kuKernelFlushCaches((void *)h->addr,sizeof(h->orig_instr));
}
static void repatch_hook(const so_hook *h) {
    sceClibMemcpy((void *)h->addr,h->patch_instr,sizeof(h->patch_instr));
    kuKernelFlushCaches((void *)h->addr,sizeof(h->patch_instr));
}
static uintptr_t hook_target(const so_hook *h) { return h->thumb_addr?h->thumb_addr:h->addr; }

static void continue_play(void *self,const void *filename,bool a,bool b) {
    restore_hook(&base_play_hook);
    ((void (*)(void *,const void *,bool,bool))hook_target(&base_play_hook))(self,filename,a,b);
    repatch_hook(&base_play_hook);
}
static void continue_self(const so_hook *h,void *self) {
    restore_hook(h);
    ((void (*)(void *))hook_target(h))(self);
    repatch_hook(h);
}
static void continue_volume(void *self,float value) {
    restore_hook(&base_volume_hook);
    ((void (*)(void *,float))hook_target(&base_volume_hook))(self,value);
    repatch_hook(&base_volume_hook);
}

static void hooked_base_play(void *self,const void *filename,bool a,bool b) {
    const char *path=string_data?string_data(filename):NULL;
    const char *track=known_track(path);
    if(track) {
        static unsigned reports;
        if(__atomic_fetch_add(&reports,1,__ATOMIC_RELAXED)<16)
            l_perf("music_ogg request path=%s arg_a=%d arg_b=%d owner=%p",path?path:"(null)",a,b,self);
        /* All five shipped tracks are music/ambience loops. The engine stop
         * hook still controls transitions, so this avoids depending on the
         * undocumented meaning of the two BaseStream::play bools. */
        music_claim_and_play(self,track,1);
        return;
    }
    continue_play(self,filename,a,b);
}

static void hooked_base_stop(void *self) {
    if(owner_claimed(self)) { music_stop_owner(self); return; }
    continue_self(&base_stop_hook,self);
}
static void hooked_base_pause(void *self) {
    if(owner_claimed(self)) { music_pause_owner(self,1); return; }
    continue_self(&base_pause_hook,self);
}
static void hooked_base_resume(void *self) {
    if(owner_claimed(self)) { music_pause_owner(self,0); return; }
    continue_self(&base_resume_hook,self);
}
static void hooked_base_volume(void *self,float value) {
    if(owner_claimed(self)) { music_volume_owner(self,value); return; }
    continue_volume(self,value);
}

static void install_optional_hook(const char *symbol,uintptr_t replacement,so_hook *out,unsigned bit,unsigned *mask) {
    uintptr_t addr=(uintptr_t)so_symbol(&so_mod,symbol);
    if(!addr) return;
    *out=hook_addr(addr,replacement);
    if(out->addr) *mask|=bit;
}

static void install_ogg_backend(void) {
    string_data=(void *)so_symbol(&so_mod,"_ZNK6STRING5c_strEv");
    uintptr_t play=(uintptr_t)so_symbol(&so_mod,"_ZN5sound10BaseStream4playERK6STRINGbb");
    if(!string_data || !play) {
        l_perf("audio_stream installed=0 music_mode=2 reason=BaseStream_play_or_STRING_c_str_not_exported play=%p c_str=%p",
               (void *)play,(void *)string_data);
        return;
    }
    base_play_hook=hook_addr(play,(uintptr_t)hooked_base_play);
    if(!base_play_hook.addr) {
        l_perf("audio_stream installed=0 music_mode=2 reason=BaseStream_play_hook_failed");
        return;
    }
    unsigned optional=0;
    install_optional_hook("_ZN5sound10BaseStream4stopEv",(uintptr_t)hooked_base_stop,&base_stop_hook,1,&optional);
    install_optional_hook("_ZN5sound10BaseStream5pauseEv",(uintptr_t)hooked_base_pause,&base_pause_hook,2,&optional);
    install_optional_hook("_ZN5sound10BaseStream6resumeEv",(uintptr_t)hooked_base_resume,&base_resume_hook,4,&optional);
    install_optional_hook("_ZN5sound10BaseStream9setVolumeEf",(uintptr_t)hooked_base_volume,&base_volume_hook,8,&optional);
    l_perf("audio_stream installed=1 music_mode=2 backend=ogg_vorbis_existing_sceAudioOut play=%p optional_hooks=0x%X voices=%d",
           (void *)play,optional,MUSIC_VOICE_COUNT);
}

void audio_stream_install(void) {
    if(setting_music_mode==2) {
        install_ogg_backend();
        return;
    }
    if(setting_music_mode==1) {
        install_legacy_pcm_hook();
        return;
    }
    l_perf("audio_stream installed=0 music_mode=0 backend=stable_silent_fallback music_hook_skipped=1");
}
