/* MIT. Canonical MAP children: inclusive probes, no gameplay changes. */
#include <stdint.h>
#include <string.h>
#include <so_util/so_util.h>
#include <kubridge.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h>
#include "utils/engine_probe.h"
#include "utils/logger.h"
extern so_module so_mod;
typedef struct { unsigned calls,samples,us,max_us,hist[8];uintptr_t original;int installed; } Probe;
static Probe probes[12];
static const unsigned edges[]={100,500,1000,5000,10000,33333,50000,UINT32_MAX};
static void record(unsigned i,unsigned us) {
    Probe *p=probes+i;__atomic_fetch_add(&p->samples,1,__ATOMIC_RELAXED);__atomic_fetch_add(&p->us,us,__ATOMIC_RELAXED);
    unsigned old=__atomic_load_n(&p->max_us,__ATOMIC_RELAXED);
    while(us>old && !__atomic_compare_exchange_n(&p->max_us,&old,us,1,__ATOMIC_RELAXED,__ATOMIC_RELAXED)) {}
    for(unsigned b=0;b<8;b++) if(us<=edges[b]) { __atomic_fetch_add(&p->hist[b],1,__ATOMIC_RELAXED);break; }
}
static uintptr_t probe_start(void *self) {
    unsigned sequence=__atomic_fetch_add(&probes[0].calls,1,__ATOMIC_RELAXED);
    uint64_t start=(sequence&0u)==0?sceKernelGetProcessTimeWide():0;
    uintptr_t result=((uintptr_t (*)(void *))probes[0].original)(self);
    if(start) record(0,(unsigned)(sceKernelGetProcessTimeWide()-start));return result;
}
static uintptr_t probe_loader(void *self) {
    unsigned sequence=__atomic_fetch_add(&probes[1].calls,1,__ATOMIC_RELAXED);
    uint64_t start=(sequence&0u)==0?sceKernelGetProcessTimeWide():0;
    uintptr_t result=((uintptr_t (*)(void *))probes[1].original)(self);
    if(start) record(1,(unsigned)(sceKernelGetProcessTimeWide()-start));return result;
}
static uintptr_t probe_script(void *self) {
    unsigned sequence=__atomic_fetch_add(&probes[2].calls,1,__ATOMIC_RELAXED);
    uint64_t start=(sequence&0u)==0?sceKernelGetProcessTimeWide():0;
    uintptr_t result=((uintptr_t (*)(void *))probes[2].original)(self);
    if(start) record(2,(unsigned)(sceKernelGetProcessTimeWide()-start));return result;
}
static uintptr_t probe_squeeze(void *self) {
    unsigned sequence=__atomic_fetch_add(&probes[3].calls,1,__ATOMIC_RELAXED);
    uint64_t start=(sequence&0u)==0?sceKernelGetProcessTimeWide():0;
    uintptr_t result=((uintptr_t (*)(void *))probes[3].original)(self);
    if(start) record(3,(unsigned)(sceKernelGetProcessTimeWide()-start));return result;
}
static uintptr_t probe_damaged(void *self) {
    unsigned sequence=__atomic_fetch_add(&probes[4].calls,1,__ATOMIC_RELAXED);
    uint64_t start=(sequence&0u)==0?sceKernelGetProcessTimeWide():0;
    uintptr_t result=((uintptr_t (*)(void *))probes[4].original)(self);
    if(start) record(4,(unsigned)(sceKernelGetProcessTimeWide()-start));return result;
}
static uintptr_t probe_delete_sprites(void *self) {
    unsigned sequence=__atomic_fetch_add(&probes[5].calls,1,__ATOMIC_RELAXED);
    uint64_t start=(sequence&0u)==0?sceKernelGetProcessTimeWide():0;
    uintptr_t result=((uintptr_t (*)(void *))probes[5].original)(self);
    if(start) record(5,(unsigned)(sceKernelGetProcessTimeWide()-start));return result;
}
static uintptr_t probe_autoaim(void *self) {
    unsigned sequence=__atomic_fetch_add(&probes[6].calls,1,__ATOMIC_RELAXED);
    uint64_t start=(sequence&0u)==0?sceKernelGetProcessTimeWide():0;
    uintptr_t result=((uintptr_t (*)(void *))probes[6].original)(self);
    if(start) record(6,(unsigned)(sceKernelGetProcessTimeWide()-start));return result;
}
static uintptr_t probe_groups(void *self) {
    unsigned sequence=__atomic_fetch_add(&probes[7].calls,1,__ATOMIC_RELAXED);
    uint64_t start=(sequence&0u)==0?sceKernelGetProcessTimeWide():0;
    uintptr_t result=((uintptr_t (*)(void *))probes[7].original)(self);
    if(start) record(7,(unsigned)(sceKernelGetProcessTimeWide()-start));return result;
}
static uintptr_t probe_sfx_play(void *self,int a,int b) {
    unsigned sequence=__atomic_fetch_add(&probes[8].calls,1,__ATOMIC_RELAXED);
    uint64_t start=(sequence&7u)==0?sceKernelGetProcessTimeWide():0;
    uintptr_t result=((uintptr_t (*)(void *,int,int))probes[8].original)(self,a,b);
    if(start) record(8,(unsigned)(sceKernelGetProcessTimeWide()-start));return result;
}
static uintptr_t probe_postponed_scripts(void *self) {
    unsigned sequence=__atomic_fetch_add(&probes[9].calls,1,__ATOMIC_RELAXED);
    uint64_t start=(sequence&0u)==0?sceKernelGetProcessTimeWide():0;
    uintptr_t result=((uintptr_t (*)(void *))probes[9].original)(self);
    if(start) record(9,(unsigned)(sceKernelGetProcessTimeWide()-start));return result;
}
static uintptr_t probe_script_call(void *self,int index,const void *a,const void *b,const void *c,void *result_out) {
    unsigned sequence=__atomic_fetch_add(&probes[10].calls,1,__ATOMIC_RELAXED);
    uint64_t start=(sequence&63u)==0?sceKernelGetProcessTimeWide():0;
    uintptr_t result=((uintptr_t (*)(void *,int,const void *,const void *,const void *,void *))probes[10].original)(self,index,a,b,c,result_out);
    if(start) record(10,(unsigned)(sceKernelGetProcessTimeWide()-start));return result;
}
static uintptr_t probe_script_startup(void *self) {
    __atomic_fetch_add(&probes[11].calls,1,__ATOMIC_RELAXED);
    uint64_t start=sceKernelGetProcessTimeWide();
    uintptr_t result=((uintptr_t (*)(void *))probes[11].original)(self);
    record(11,(unsigned)(sceKernelGetProcessTimeWide()-start));return result;
}
static const struct { const char *tag,*symbol;unsigned offset;uint32_t prologue[2];uintptr_t replacement;unsigned period; } methods[]={
    {"start","_ZN3MAP9StartTactEv",0x431f58,{0xaf03b5f0,0xbd04f84d},(uintptr_t)probe_start,1},
    {"loader","_ZN9MapLoader4tactEv",0x448c14,{0xaf03b5f0,0x8d04f84d},(uintptr_t)probe_loader,1},
    {"script","_ZN6SCRIPT8mainLoopEv",0x48b930,{0xaf03b5f0,0x0b00e92d},(uintptr_t)probe_script,1},
    {"squeeze","_ZN16SPRITE_COLLECTOR7squeezeEv",0x4f15c0,{0xaf03b5f0,0x0b00e92d},(uintptr_t)probe_squeeze,1},
    {"damaged","_ZN16SPRITE_COLLECTOR18processDamagedTactEv",0x4f5970,{0xaf03b5f0,0x8d04f84d},(uintptr_t)probe_damaged,1},
    {"delete_sprites","_ZN16SPRITE_COLLECTOR22clearDeleteSpritesListEv",0x4f104c,{0xaf03b5f0,0x8d04f84d},(uintptr_t)probe_delete_sprites,1},
    {"autoaim","_ZN7AutoAim4execEv",0x3d9b2c,{0xaf03b5f0,0x0f00e92d},(uintptr_t)probe_autoaim,1},
    {"groups","_ZN3MAP10groupsTactEv",0x437a78,{0xaf03b5f0,0x0700e92d},(uintptr_t)probe_groups,1},
    {"sfx_play","_ZN5sound9SfxBuffer4playEii",0x4d3860,{0xaf03b5f0,0x0b00e92d},(uintptr_t)probe_sfx_play,8},
    {"postponed_scripts","_ZN3MAP20execPostponedScriptsEv",0x4320b0,{0xaf03b5f0,0x0700e92d},(uintptr_t)probe_postponed_scripts,1},
    {"script_call","_ZN6SCRIPT12callFunctionEiRKN6script11StackObjectES3_S3_PS1_",0x48bb84,{0xaf03b5f0,0x0f00e92d},(uintptr_t)probe_script_call,64},
    {"script_startup","_ZN6SCRIPT7startupEv",0x48ba58,{0xaf03b5f0,0x0b00e92d},(uintptr_t)probe_script_startup,1},
};

void map_profile_install(void) {
    unsigned mask=0;
    for(unsigned i=0;i<12;i++) {
        uintptr_t address=(uintptr_t)so_symbol(&so_mod,methods[i].symbol),entry=address&~1u;
        uintptr_t arena=(so_mod.patch_head+3)&~3u;uint32_t code[4];
        if(address!=so_mod.load_addr+methods[i].offset+1 || arena<so_mod.patch_base || arena>so_mod.patch_base+so_mod.patch_size ||
           so_mod.patch_base+so_mod.patch_size-arena<16 || !engine_probe_trampoline(code,(void *)entry,methods[i].prologue,(uint32_t)(entry+8))) continue;
        sceClibMemcpy((void *)arena,code,16);probes[i].original=arena|1;so_mod.patch_head=arena+16;
        hook_addr(address,methods[i].replacement);kuKernelFlushCaches((void *)arena,16);kuKernelFlushCaches((void *)entry,8);
        probes[i].installed=1;mask|=1u<<i;
    }
    l_perf("map_profile installed_mask=0x%X expected_mask=0xFFF inclusive=1 sfx_sample_period=8",mask);
}
void map_profile_report(void) {
    static Probe previous[12];
    for(unsigned i=0;i<12;i++) {
        Probe *p=probes+i;unsigned calls=__atomic_load_n(&p->calls,__ATOMIC_RELAXED),samples=__atomic_load_n(&p->samples,__ATOMIC_RELAXED),us=__atomic_load_n(&p->us,__ATOMIC_RELAXED);
        unsigned n=samples-previous[i].samples,sum=0,p95=0;
        for(unsigned b=0;b<8;b++) { unsigned h=__atomic_load_n(&p->hist[b],__ATOMIC_RELAXED);sum+=h-previous[i].hist[b];previous[i].hist[b]=h;
            if(!p95 && n && (uint64_t)sum*100>=(uint64_t)n*95) p95=edges[b]; }
        l_perf("map_subphase name=%s installed=%d calls=%u sampled_calls=%u sampled_us=%u estimated_us=%llu avg_sample_us=%u max_us_lifetime=%u p95_bucket_upper_us=%u sample_period=%u inclusive=1",
            methods[i].tag,p->installed,calls-previous[i].calls,n,us-previous[i].us,(unsigned long long)(us-previous[i].us)*methods[i].period,n?(us-previous[i].us)/n:0,
            __atomic_load_n(&p->max_us,__ATOMIC_RELAXED),p95,methods[i].period);
        previous[i].calls=calls;previous[i].samples=samples;previous[i].us=us;
    }
}
