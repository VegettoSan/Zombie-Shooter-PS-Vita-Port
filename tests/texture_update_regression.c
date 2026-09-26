#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static size_t copied;
static void copy_checked(void *d,const void *s,size_t n) { copied+=n; memcpy(d,s,n); }
#define ZOMBIE_TEXTURE_COPY copy_checked
#include "utils/zombie_texture_update.h"
static void trial(unsigned ow,unsigned oh,unsigned x,unsigned y,unsigned w,unsigned h,unsigned bpp) {
    size_t stride=((ow+7)&~7u)*bpp, n=stride*oh;
    unsigned char *old=malloc(n+64),*fast=malloc(n+64),*expected=malloc(n+64);
    assert(old && fast && expected);
    for(size_t i=0;i<n+64;++i) old[i]=(i*23+17)&255;
    memset(fast,0xcd,n+64); memcpy(expected,old,n); memset(expected+n,0xcd,64);
    copied=0;
    size_t kept=zombie_texture_preserve_native(fast,old,stride,oh,x,y,w,h,bpp);
    assert(kept==n-(size_t)w*h*bpp && copied==kept);
    for(unsigned row=y;row<y+h;++row) {
        /* Simulate exact native RGBA upload to both implementations. */
        memset(fast+row*stride+x*bpp,0xa5,w*bpp);
        memset(expected+row*stride+x*bpp,0xa5,w*bpp);
    }
    assert(memcmp(fast,expected,n+64)==0); /* Includes padding and guard bytes. */
    if(x==0 && y==0 && w==ow && h==oh && (ow&7)==0) assert(copied==0);
    free(old);free(fast);free(expected);
}
int main(void) {
    for(unsigned bpp=2;bpp<=4;bpp+=2) {
    for(unsigned ow=1;ow<=24;++ow) for(unsigned oh=1;oh<=12;++oh) {
        trial(ow,oh,0,0,ow,oh,bpp);
        for(unsigned x=0;x<ow;++x) for(unsigned y=0;y<oh;++y) {
            trial(ow,oh,x,y,ow-x,oh-y,bpp);
            trial(ow,oh,x,y,1,1,bpp);
        }
    }
    trial(960,544,0,0,960,544,bpp);
    trial(1024,1024,0,0,960,544,bpp);
    trial(1024,1024,511,511,17,13,bpp);
    trial(256,256,0,0,256,256,bpp);
    }
    puts("Texture regression passed: native RGBA/RGB565, full/partial replacement, NPOT padding, every edge, guards and byte preservation equivalence");
}
