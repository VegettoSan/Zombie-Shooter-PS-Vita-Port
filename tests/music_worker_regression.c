#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ZOMBIE_MUSIC_HOST 1
static uint64_t sceKernelGetProcessTimeWide(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000; }
static void sceKernelDelayThread(unsigned us) { struct timespec t={us/1000000,(us%1000000)*1000};nanosleep(&t,NULL); }
#define l_perf(...) ((void)0)
#include "../source/utils/audio_stream.c"
int setting_music_mode=2;
static const char *get_string(const void *p) { return p; }
static unsigned stops;
static void stop_native(void *self) { music_close(self);*((char *)self+28)=0;stops++; }
static void wait_ready(void *self) {
    uint64_t start=sceKernelGetProcessTimeWide();
    for(;;) { pthread_mutex_lock(&music_mutex);int i=find_owner(self);int ready=i>=0 && voices[i].count>=1024;int failed=i>=0 && voices[i].failed;pthread_mutex_unlock(&music_mutex);
        assert(!failed);if(ready) return;assert(sceKernelGetProcessTimeWide()-start<2000000);sceKernelDelayThread(1000); }
}
int main(void) {
    unsigned char owner[72]={0};int volume=100;memcpy(owner+32,&volume,4);owner[40]=1;
    string_data=get_string;native_stop=stop_native;worker_stop=0;
    assert(!pthread_create(&worker,NULL,music_worker,NULL));worker_valid=1;
    assert(music_open(owner,"music\\menu_mus01.ogg"));wait_ready(owner);
    int16_t out[256]={0};zombie_music_mix(out,128);
    music_pause(owner);pthread_mutex_lock(&music_mutex);int i=find_owner(owner);unsigned queued=voices[i].count;pthread_mutex_unlock(&music_mutex);
    memset(out,0,sizeof(out));zombie_music_mix(out,128);for(int j=0;j<256;j++) assert(!out[j]);
    pthread_mutex_lock(&music_mutex);assert(voices[i].count>=queued);pthread_mutex_unlock(&music_mutex);
    music_resume(owner);music_volume(owner,0);memset(out,0,sizeof(out));zombie_music_mix(out,128);for(int j=0;j<256;j++) assert(!out[j]);
    music_volume(owner,100);
    // Large simultaneous mixed samples always clamp to signed16, never wrap.
    pthread_mutex_lock(&music_mutex);voices[i].read=0;voices[i].write=128;voices[i].count=128;voices[i].ready=1;
    for(int j=0;j<256;j++) voices[i].pcm[j]=32767;pthread_mutex_unlock(&music_mutex);
    for(int j=0;j<256;j++) out[j]=32767;zombie_music_mix(out,128);for(int j=0;j<256;j++) assert(out[j]==32767);
    music_close(owner);pthread_mutex_lock(&music_mutex);assert(find_owner(owner)==-1);pthread_mutex_unlock(&music_mutex); // replaced below by real lookup
    for(int n=0;n<40;n++) { assert(music_open(owner,n%2?"music/mus01.m4a":"music/menu_mus01.m4a"));music_close(owner); }
    assert(music_open(owner,"music/menu_mus01.m4a"));wait_ready(owner);
    pthread_mutex_lock(&music_mutex);i=find_owner(owner);voices[i].failed=1;pthread_mutex_unlock(&music_mutex);
    music_update(owner);assert(stops==1);
    zombie_music_shutdown();assert(!worker_valid);
    // SDL_open starts the backend again after its initial SDL_close.
    music_backend_installed=1;zombie_music_start();assert(worker_valid);
    assert(music_open(owner,"music/menu_mus01.m4a"));wait_ready(owner);
    zombie_music_shutdown();assert(!worker_valid);
    puts("Music worker PASS: original path, generation cancellation, pause/resume, volume, clipping, stop/error, destroy/shutdown");
}
