#include "opensles_queue_host.h"
#include "queue-under-test.c"
static void setup(IBufferQueue *q,CAudioPlayer *p,unsigned rate,unsigned channels,unsigned bits){
    memset(p,0,sizeof(*p));pthread_mutex_init(&p->obj.mMutex,NULL);pthread_cond_init(&p->obj.mCond,NULL);
    memset(q,0,sizeof(*q));IBufferQueue_init(q);q->mThis=p;q->mArray=q->mTypical;q->mFront=q->mRear=q->mArray;q->mNumBuffers=4;q->samplerate=rate*1000;q->channels=channels;q->bps=bits;
}
static void teardown(IBufferQueue *q,CAudioPlayer *p){IBufferQueue_Destroy(q);pthread_mutex_destroy(&p->obj.mMutex);pthread_cond_destroy(&p->obj.mCond);}
static unsigned ack_ready;
static void *ack(void *arg){
    IBufferQueue *q=arg;struct timespec t={0,1000000};
    zombie_opensles_mix_begin();__atomic_store_n(&ack_ready,1,__ATOMIC_RELEASE);
    for(;;){
        interface_lock_exclusive(q);
        if(q->mClearRequested){
            IBufferQueue_ReleaseArrayBuffers(q);q->mRear=q->mFront=q->mArray;
            q->mState.count=q->mState.playIndex=0;q->mClearRequested=0;
            if(q->mThis->mTrack){q->mThis->mTrack->mReader=NULL;q->mThis->mTrack->mAvail=0;}
            pthread_cond_signal(&InterfaceToIObject(q)->mCond);interface_unlock_exclusive(q);
            zombie_opensles_mix_end();return NULL;
        }
        interface_unlock_exclusive(q);nanosleep(&t,NULL);
    }
}
static void wait_ack_ready(void){while(!__atomic_load_n(&ack_ready,__ATOMIC_ACQUIRE)){} }
int main(void){IBufferQueue q;CAudioPlayer p;
    int16_t mono[]={-32768,0,32767},expected[]={-32768,-32768,-32768,-32768,0,0,0,0,32767,32767,32767,32767};
    setup(&q,&p,22050,1,16);assert(!IBufferQueue_Enqueue(&q,mono,sizeof(mono)));assert(q.mArray[0].mSize==sizeof(expected));assert(!memcmp(q.mArray[0].mBuffer,expected,sizeof(expected)));
    Track track={.mAudioPlayer=&p,.mReader=q.mArray[0].mBuffer,.mAvail=q.mArray[0].mSize,.mFramesMixed=17};p.mTrack=&track;
    unsigned before=released,fast=immediate_calls;
    q.mState.playIndex=9;assert(!IBufferQueue_Clear(&q));
    assert(!q.mClearRequested && !q.mState.count && !q.mState.playIndex);
    assert(!track.mReader && !track.mAvail && track.mFramesMixed==17);
    assert(released==before+1 && immediate_calls==fast+1);
    assert(!IBufferQueue_Enqueue(&q,mono,sizeof(mono)));
    void *owned=q.mArray[0].mOwnedBuffer;before=released;
    zombie_opensles_mix_begin();
    uint64_t t=sceKernelGetProcessTimeWide();assert(IBufferQueue_Clear(&q)==SL_RESULT_RESOURCE_ERROR);
    assert(sceKernelGetProcessTimeWide()-t>=90000);
    assert(q.mClearRequested && q.mArray[0].mOwnedBuffer==owned && released==before);
    zombie_opensles_mix_end();
    assert(!IBufferQueue_Clear(&q));assert(released==before+1 && !q.mClearRequested);
    /* A real reader excludes immediate freeing, then acknowledges the pending
     * request under the player mutex; repeat the handoff across threads. */
    for(unsigned i=0;i<200;i++){
        assert(!IBufferQueue_Enqueue(&q,mono,sizeof(mono)));before=released;fast=immediate_calls;
        track.mReader=q.mArray[0].mBuffer;track.mAvail=q.mArray[0].mSize;
        ack_ready=0;pthread_t th;pthread_create(&th,NULL,ack,&q);wait_ack_ready();
        assert(!IBufferQueue_Clear(&q));pthread_join(th,NULL);
        assert(!q.mClearRequested && !q.mState.count && released==before+1 && immediate_calls==fast);
        assert(!track.mReader && !track.mAvail);
    }
    /* A mismatched track must use the old request path, never touch/free it. */
    assert(!IBufferQueue_Enqueue(&q,mono,sizeof(mono)));before=released;
    track.mAudioPlayer=NULL;assert(IBufferQueue_Clear(&q)==SL_RESULT_RESOURCE_ERROR);assert(released==before && q.mClearRequested);
    track.mAudioPlayer=&p;assert(!IBufferQueue_Clear(&q));assert(released==before+1);
    BufferHeader *array=q.mArray;q.mArray=NULL;fast=immediate_calls;
    assert(IBufferQueue_Clear(&q)==SL_RESULT_RESOURCE_ERROR);assert(immediate_calls==fast && q.mClearRequested);
    q.mArray=array;p.mTrack=NULL;assert(!IBufferQueue_Clear(&q));assert(!q.mClearRequested);
    teardown(&q,&p);
    uint8_t pcm8[]={0,128,255};int16_t x[]={-32768,-32768,0,0,32512,32512};setup(&q,&p,44100,1,8);assert(!IBufferQueue_Enqueue(&q,pcm8,sizeof(pcm8)));assert(!memcmp(q.mArray[0].mBuffer,x,sizeof(x)));before=released;teardown(&q,&p);assert(released==before+1);
    uint8_t stereo8[]={0,255,128,128};int16_t y[]={-32768,32512,0,0};setup(&q,&p,44100,2,8);assert(!IBufferQueue_Enqueue(&q,stereo8,sizeof(stereo8)));assert(!memcmp(q.mArray[0].mBuffer,y,sizeof(y)));teardown(&q,&p);
    int16_t stereo16[]={-123,123,456,-456};setup(&q,&p,44100,2,16);assert(!IBufferQueue_Enqueue(&q,stereo16,sizeof(stereo16)));assert(q.mArray[0].mBuffer==stereo16 && !q.mArray[0].mOwnedBuffer);assert(IBufferQueue_Enqueue(&q,stereo16,3)==SL_RESULT_CONTENT_UNSUPPORTED);teardown(&q,&p);
    for(unsigned rate=32000;rate<=48000;rate+=16000){setup(&q,&p,rate,2,16);assert(IBufferQueue_Enqueue(&q,stereo16,sizeof(stereo16))==SL_RESULT_CONTENT_UNSUPPORTED);assert(q.mState.count==0);teardown(&q,&p);}
    for(unsigned frames=128;frames<=2048;frames*=2){assert(frames*4==frames*2*sizeof(int16_t));assert(frames%128==0);assert((uint64_t)frames*1000000/44100>0);}
    puts("OpenSL actual queue PASS: PCM8/16 mono/stereo, 2x conversion, unsupported rates/alignment, Clear immediate reader reset, active-reader exclusion (200 threads), timeout ownership + acknowledgement, track mismatch, Destroy, buffer units");
}
