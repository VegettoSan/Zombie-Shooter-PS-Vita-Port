#include <assert.h>
#include <stdio.h>
#include "../source/utils/render_scale_policy.h"
int main(void) {
    float android=1024.0f/960.0f;
    float f=render_scale_cap(960,544,android,864);
    assert((unsigned)(960*f)==864);
    assert((unsigned)(544*f)==489);
    assert(render_scale_cap(960,544,android,960)==1.0f);
    assert(render_scale_cap(960,544,android,0)==android);
    assert(render_scale_cap(960,544,0.75f,864)==0.75f);
    assert(render_scale_cap(544,960,android,864)==android);
    assert(render_scale_cap(960,544,android,100)==android);
    assert(isnan(render_scale_cap(960,544,NAN,864)));
    assert(render_scale_cap(960,544,-1,864)==-1);
    puts("render scale policy: PASS (864x489 / 960x544; preserve smaller surfaces and invalid config)");
}
