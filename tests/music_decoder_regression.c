#include "utils/music_decoder.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv) {
    assert(argc==4);MusicDecoder *d=music_decoder_open(argv[1]);assert(d);
    FILE *f=fopen(argv[2],"wb");assert(f);int16_t samples[2050];unsigned long long frames=0;
    int n;while((n=music_decoder_read(d,samples,1025,0))>0) {fwrite(samples,4,(size_t)n,f);frames+=(unsigned)n;}
    assert(n==0 && music_decoder_read(d,samples,100,0)==0);fclose(f);music_decoder_destroy(d);
    d=music_decoder_open(argv[1]);assert(d);
    unsigned long long target=frames+44100,total=0;
    while(total<target) {unsigned wanted=target-total<1025?(unsigned)(target-total):1025;n=music_decoder_read(d,samples,wanted,1);assert(n==(int)wanted);total+=(unsigned)n;}
    music_decoder_destroy(d);assert(!music_decoder_open(argv[3]));
    printf("Original AAC decoder PASS frames=%llu loop_frames=%llu\n",frames,total);
}
