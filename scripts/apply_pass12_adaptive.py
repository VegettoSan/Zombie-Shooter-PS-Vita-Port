#!/usr/bin/env python3
from pathlib import Path

p = Path('source/patch.c')
s = p.read_text()


def replace_once(old: str, new: str, label: str) -> None:
    global s
    if new in s:
        return
    n = s.count(old)
    if n != 1:
        raise SystemExit(f'{label}: expected one anchor, found {n}')
    s = s.replace(old, new, 1)


replace_once(
'''/* Pass 12: canonical MAP::tact calls softwareTact at SO+0x437580 and
 * GRAPH::Tact immediately after at SO+0x43758C. Static call-graph analysis
 * shows GRAPH::Tact is render-only (DrawLayer/batch/state calls), so reuse it
 * on the exact same alternate ticks. MAP logic, input, scripts, multiplayer
 * and SPRITE_COLLECTOR::squeeze still execute every engine tick. */
volatile int zombie_render_reuse_active_this_tick = 0;
static uintptr_t render_reuse_last_software_result;
static uintptr_t render_reuse_last_graph_result;
static unsigned render_reuse_phase;
static unsigned render_reuse_rendered;
static unsigned render_reuse_reused;
static unsigned graph_rendered;
static unsigned graph_reused;
static int render_reuse_has_frame;
static int render_reuse_has_graph;
''',
'''/* Pass 12 adaptive pacing. Canonical MAP::tact calls softwareTact at
 * SO+0x437580 and GRAPH::Tact immediately after at SO+0x43758C. GRAPH::Tact
 * is render-only, so both render phases may reuse their previous result while
 * MAP logic/input/scripts/multiplayer continue every tick. The number of reuse
 * ticks is chosen from measured full-vs-reuse MAP time to target 33.333 ms. */
#define RENDER_REUSE_TARGET_US 33333u
#define RENDER_REUSE_MAX_SKIPS 3u
volatile int zombie_render_reuse_active_this_tick = 0;
static uintptr_t render_reuse_last_software_result;
static uintptr_t render_reuse_last_graph_result;
static unsigned render_reuse_skip_remaining;
static unsigned render_reuse_next_skips;
static unsigned render_reuse_rendered;
static unsigned render_reuse_reused;
static unsigned graph_rendered;
static unsigned graph_reused;
static unsigned render_reuse_map_render_us;
static unsigned render_reuse_map_reuse_us;
static unsigned render_reuse_cycles[RENDER_REUSE_MAX_SKIPS + 1];
static int render_reuse_has_frame;
static int render_reuse_has_graph;
''', 'variables')

replace_once(
'''static uintptr_t probe_software(void *self,int argument) {
    int reuse = setting_software_frameskip && render_reuse_has_frame &&
                ((render_reuse_phase++ & 1u) != 0);
    if (reuse) {
        zombie_render_reuse_active_this_tick = 1;
        render_reuse_reused++;
        return render_reuse_last_software_result;
    }
    zombie_render_reuse_active_this_tick = 0;
    uint64_t start=sceKernelGetProcessTimeWide();
    uintptr_t ret=((uintptr_t (*)(void *,int))engine_original[PERF_ENGINE_SOFTWARE])(self,argument);
    perf_engine_phase(PERF_ENGINE_SOFTWARE,start);
    render_reuse_last_software_result=ret;
    render_reuse_has_frame=1;
    render_reuse_rendered++;
    return ret;
}
''',
'''static uintptr_t probe_software(void *self,int argument) {
    int reuse = setting_software_frameskip && render_reuse_has_frame &&
                render_reuse_skip_remaining != 0;
    if (reuse) {
        render_reuse_skip_remaining--;
        zombie_render_reuse_active_this_tick = 1;
        render_reuse_reused++;
        return render_reuse_last_software_result;
    }
    zombie_render_reuse_active_this_tick = 0;
    uint64_t start=sceKernelGetProcessTimeWide();
    uintptr_t ret=((uintptr_t (*)(void *,int))engine_original[PERF_ENGINE_SOFTWARE])(self,argument);
    perf_engine_phase(PERF_ENGINE_SOFTWARE,start);
    render_reuse_last_software_result=ret;
    render_reuse_has_frame=1;
    render_reuse_rendered++;
    return ret;
}
''', 'software probe')

replace_once(
'''void render_reuse_report(void) {
    static unsigned old_rendered,old_reused,old_graph_rendered,old_graph_reused;
    unsigned rendered=render_reuse_rendered, reused=render_reuse_reused;
    unsigned gr=graph_rendered, gu=graph_reused;
    l_perf("render_reuse config=%d rendered_ticks=%u reused_ticks=%u graph_rendered_ticks=%u graph_reused_ticks=%u has_frame=%d has_graph=%d current_reuse=%d strategy=software_graph_2to1",
        setting_software_frameskip,rendered-old_rendered,reused-old_reused,
        gr-old_graph_rendered,gu-old_graph_reused,render_reuse_has_frame,
        render_reuse_has_graph,zombie_render_reuse_active_this_tick);
    old_rendered=rendered;old_reused=reused;old_graph_rendered=gr;old_graph_reused=gu;
}

ENGINE_THIS_PROBE(map,PERF_ENGINE_MAP)
''',
'''static unsigned render_reuse_choose_skips(unsigned full_us,unsigned reuse_us) {
    if (!setting_software_frameskip || full_us <= RENDER_REUSE_TARGET_US)
        return 0;
    /* First overload gets one reuse tick so the next MAP call measures the
     * non-render residual cost. If residual logic itself misses 33.3 ms,
     * repeated frames cannot solve it; stay conservative at one reuse tick. */
    if (!reuse_us || reuse_us >= RENDER_REUSE_TARGET_US)
        return 1;
    uint64_t numerator=(uint64_t)full_us-RENDER_REUSE_TARGET_US;
    uint64_t denominator=(uint64_t)RENDER_REUSE_TARGET_US-reuse_us;
    unsigned skips=(unsigned)((numerator+denominator-1)/denominator);
    if (skips < 1) skips=1;
    if (skips > RENDER_REUSE_MAX_SKIPS) skips=RENDER_REUSE_MAX_SKIPS;
    return skips;
}

static uintptr_t probe_map(void *self) {
    uint64_t start=sceKernelGetProcessTimeWide();
    uintptr_t ret=((uintptr_t (*)(void *))engine_original[PERF_ENGINE_MAP])(self);
    unsigned us=(unsigned)(sceKernelGetProcessTimeWide()-start);
    perf_engine_phase(PERF_ENGINE_MAP,start);
    if (setting_software_frameskip && zombie_render_reuse_active_this_tick) {
        render_reuse_map_reuse_us=us;
    } else {
        render_reuse_map_render_us=us;
        unsigned skips=render_reuse_choose_skips(us,render_reuse_map_reuse_us);
        render_reuse_next_skips=skips;
        render_reuse_skip_remaining=skips;
        render_reuse_cycles[skips]++;
    }
    return ret;
}

void render_reuse_report(void) {
    static unsigned old_rendered,old_reused,old_graph_rendered,old_graph_reused;
    static unsigned old_cycles[RENDER_REUSE_MAX_SKIPS + 1];
    unsigned rendered=render_reuse_rendered, reused=render_reuse_reused;
    unsigned gr=graph_rendered, gu=graph_reused;
    unsigned c0=render_reuse_cycles[0]-old_cycles[0];
    unsigned c1=render_reuse_cycles[1]-old_cycles[1];
    unsigned c2=render_reuse_cycles[2]-old_cycles[2];
    unsigned c3=render_reuse_cycles[3]-old_cycles[3];
    l_perf("render_reuse config=%d rendered_ticks=%u reused_ticks=%u graph_rendered_ticks=%u graph_reused_ticks=%u has_frame=%d has_graph=%d current_reuse=%d strategy=software_graph_adaptive30 target_us=%u next_reuse=%u skip_remaining=%u map_render_us_last=%u map_reuse_us_last=%u cycles0=%u cycles1=%u cycles2=%u cycles3=%u",
        setting_software_frameskip,rendered-old_rendered,reused-old_reused,
        gr-old_graph_rendered,gu-old_graph_reused,render_reuse_has_frame,
        render_reuse_has_graph,zombie_render_reuse_active_this_tick,
        RENDER_REUSE_TARGET_US,render_reuse_next_skips,render_reuse_skip_remaining,
        render_reuse_map_render_us,render_reuse_map_reuse_us,c0,c1,c2,c3);
    old_rendered=rendered;old_reused=reused;old_graph_rendered=gr;old_graph_reused=gu;
    for (unsigned i=0;i<=RENDER_REUSE_MAX_SKIPS;++i) old_cycles[i]=render_reuse_cycles[i];
}
''', 'map/report')

p.write_text(s)
print('Pass12 adaptive source transform complete')
