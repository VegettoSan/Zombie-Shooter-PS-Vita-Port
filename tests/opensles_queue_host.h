#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t SLuint32;typedef int SLresult;typedef int SLboolean;
#define SL_BOOLEAN_FALSE 0
#define SL_BOOLEAN_TRUE 1
#define SL_RESULT_SUCCESS 0
#define SL_RESULT_PARAMETER_INVALID 1
#define SL_RESULT_BUFFER_INSUFFICIENT 2
#define SL_RESULT_CONTENT_UNSUPPORTED 3
#define SL_RESULT_RESOURCE_ERROR 4
#define SL_RESULT_PRECONDITIONS_VIOLATED 5
#define SL_OBJECTID_AUDIOPLAYER 1
#define SL_OBJECTID_AUDIORECORDER 2
#define SL_PLAYSTATE_STOPPED 0
#define SL_PLAYSTATE_PLAYING 1
#define ATTR_ENQUEUE 1
#define ATTR_NONE 0
#define BUFFER_HEADER_TYPICAL 4
#define USE_OUTPUTMIXEXT 1
#define SL_ENTER_INTERFACE SLresult result;
#define SL_LEAVE_INTERFACE return result;
typedef struct {const void *mBuffer;unsigned mSize;void *mOwnedBuffer;} BufferHeader;
static unsigned released;
static void BufferHeader_reset(BufferHeader *b){memset(b,0,sizeof(*b));}
static void BufferHeader_release(BufferHeader *b){if(b->mOwnedBuffer){released++;free(b->mOwnedBuffer);}BufferHeader_reset(b);}
typedef struct {pthread_mutex_t mMutex;pthread_cond_t mCond;} IObject;
typedef struct {unsigned count,playIndex;} SLBufferQueueState;
typedef struct CAudioPlayer CAudioPlayer;
typedef struct {CAudioPlayer *mAudioPlayer;const void *mReader;unsigned mAvail,mFramesMixed;} Track;
struct CAudioPlayer {IObject obj;struct {unsigned mState;int mHeadAtEnd,mHeadStalled;} mPlay;Track *mTrack;};
typedef struct {IObject obj;struct {unsigned mState;} mRecord;} CAudioRecorder;
struct SLBufferQueueItf_;
typedef void *SLBufferQueueItf;typedef void (*slBufferQueueCallback)(SLBufferQueueItf,void *);
typedef struct {const struct SLBufferQueueItf_ *mItf;CAudioPlayer *mThis;BufferHeader *mArray,*mRear,*mFront,mTypical[5];unsigned mNumBuffers,mSizeConsumed,samplerate,channels,bps;SLBufferQueueState mState;SLboolean mClearRequested;slBufferQueueCallback mCallback;void *mContext;} IBufferQueue;
struct SLBufferQueueItf_ {SLresult (*enqueue)(SLBufferQueueItf,const void *,SLuint32);SLresult (*clear)(SLBufferQueueItf);SLresult (*state)(SLBufferQueueItf,SLBufferQueueState *);SLresult (*callback)(SLBufferQueueItf,slBufferQueueCallback,void *);};
#define InterfaceToObjectID(p) SL_OBJECTID_AUDIOPLAYER
#define InterfaceToIObject(p) (&((IBufferQueue *)(p))->mThis->obj)
#define interface_lock_exclusive(p) pthread_mutex_lock(&InterfaceToIObject(p)->mMutex)
#define interface_lock_shared(p) interface_lock_exclusive(p)
#define interface_unlock_exclusive(p) pthread_mutex_unlock(&InterfaceToIObject(p)->mMutex)
#define interface_unlock_shared(p) interface_unlock_exclusive(p)
#define interface_unlock_exclusive_attributes(p,a) interface_unlock_exclusive(p)
static int _opensles_user_freq=44100;
#define audio_perf_enqueue(...) ((void)0)
#define audio_perf_call(...) ((void)0)
static unsigned immediate_calls;
#define audio_perf_clear_immediate() (++immediate_calls)
#define audio_perf_wait(...) ((void)0)
#define _log_print(...) ((void)0)
static uint64_t sceKernelGetProcessTimeWide(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
