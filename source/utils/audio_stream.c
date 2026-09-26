/* MIT. Use the original engine's OpenSL URI fallback with decoded local PCM.
 * onOpen already maps .ogg to .m4a; do not alias an unsupported codec.
 * Canonical ABI: createMusicPlayer uses hidden shared_ptr result in r0,
 * Engine this in r1 and STRING const& in r2 (0x4d20f8). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sndfile.h>
#include <so_util/so_util.h>
#include <kubridge.h>
#include <psp2/kernel/clib.h>
#include "utils/logger.h"
extern so_module so_mod;
typedef void (*CreateMusic)(void *, const void *, const void *);
static CreateMusic original;
static void (*string_init)(void *, const char *);
static void (*string_destroy)(void *);
static const char *(*string_data)(const void *);
static const char *names[]={"menu_mus01","mus01","mus02","amb01","rain"};
static unsigned reported, missing;
static void create_music(void *result,const void *engine,const void *filename) {
    const char *name=string_data(filename);
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
            (info.format&SF_FORMAT_TYPEMASK)==SF_FORMAT_WAV;
        sf_close(file);
        if(!supported) break;
        if(!(__atomic_fetch_or(&reported,1u<<i,__ATOMIC_RELAXED)&(1u<<i)))
            l_perf("audio_stream requested=%s path=%s rate=%d channels=%d frames=%llu backend=OpenSL_URI_PCM",name,path,info.samplerate,info.channels,(unsigned long long)info.frames);
        uint32_t local_string[3]; // STRING constructors/accessors prove 12 bytes.
        string_init(local_string,path);
        original(result,engine,local_string);
        string_destroy(local_string);
        return;
    }
    original(result,engine,filename);
}
void audio_stream_install(void) {
    uintptr_t addr=(uintptr_t)so_symbol(&so_mod,"_ZNK8opensles6Engine17createMusicPlayerERK6STRING");
    uintptr_t entry=addr&~(uintptr_t)1,arena=(so_mod.patch_head+3)&~(uintptr_t)3;
    const uint32_t prologue[]={0xaf03b5f0,0x0700e92d};
    string_init=(void *)so_symbol(&so_mod,"_ZN6STRINGC2EPKc");
    string_destroy=(void *)so_symbol(&so_mod,"_ZN6STRINGD1Ev");
    string_data=(void *)so_symbol(&so_mod,"_ZNK6STRING5c_strEv");
    if(!string_init || !string_destroy || !string_data || !(addr&1) || entry!=so_mod.load_addr+0x4d20f8 ||
        arena<so_mod.patch_base || arena>so_mod.patch_base+so_mod.patch_size ||
        so_mod.patch_base+so_mod.patch_size-arena<16 || memcmp((void *)entry,prologue,8)) {
        l_perf("audio_stream installed=0 reason=verified_hook_unavailable");return;
    }
    uint32_t code[]={prologue[0],prologue[1],0xf000f8df,(uint32_t)(entry+8)|1};
    sceClibMemcpy((void *)arena,code,sizeof(code));original=(CreateMusic)(arena|1);
    so_mod.patch_head=arena+sizeof(code);hook_addr(addr,(uintptr_t)create_music);
    kuKernelFlushCaches((void *)arena,sizeof(code));kuKernelFlushCaches((void *)entry,8);
    l_perf("audio_stream installed=1 original_extension_mapping=ogg_to_m4a pcm_sidecars=5");
}
