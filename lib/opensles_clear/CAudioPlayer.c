/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** \file CAudioPlayer.c AudioPlayer class */

#include "sles_allinclusive.h"
#include <time.h>

extern int zombie_opensles_backend_running(void);
#include "utils/logger.h"
#include "utils/perf.h"
#include <psp2/kernel/processmgr.h>
#if defined(ZOMBIE_Debug_AUDIO)
#define AUDIO_WARN(...) _log_print(LT_WARN, __VA_ARGS__)
#else
#define AUDIO_WARN(...) ((void)0)
#endif


/** \brief Hook called by Object::Realize when an audio player is realized */

SLresult CAudioPlayer_Realize(void *self, SLboolean async)
{
    CAudioPlayer *this = (CAudioPlayer *) self;
    SLresult result = SL_RESULT_SUCCESS;

#ifdef ANDROID
    result = android_audioPlayer_realize(this, async);
#endif

#ifdef USE_SNDFILE
    if (SL_DATALOCATOR_URI == this->mDataSource.mLocator.mLocatorType) {
        result = SndFile_Realize(this);
        /* URI player creation succeeds before format validation in Realize. */
        static unsigned uri_reports;
        if (__atomic_fetch_add(&uri_reports,1,__ATOMIC_RELAXED)<16)
            l_perf("audio_uri realize_result=%u path=%s format=0x%X rate=%d channels=%d",
                (unsigned)result,this->mSndFile.mPathname?(const char *)this->mSndFile.mPathname:"(null)",
                this->mSndFile.mSfInfo.format,this->mSndFile.mSfInfo.samplerate,this->mSndFile.mSfInfo.channels);
    }
#endif

    // At this point the channel count and sample rate might still be unknown,
    // depending on the data source and the platform implementation.
    // If they are unknown here, then they will be determined during prefetch.

    return result;
}


/** \brief Hook called by Object::Resume when an audio player is resumed */

SLresult CAudioPlayer_Resume(void *self, SLboolean async)
{
    return SL_RESULT_SUCCESS;
}


/** \brief Hook called by Object::Destroy when an audio player is destroyed */

void CAudioPlayer_Destroy(void *self)
{
    CAudioPlayer *this = (CAudioPlayer *) self;
    freeDataLocatorFormat(&this->mDataSource);
    freeDataLocatorFormat(&this->mDataSink);
    IBufferQueue_Destroy(&this->mBufferQueue);
#ifdef USE_SNDFILE
    SndFile_Destroy(this);
#endif
#ifdef ANDROID
    android_audioPlayer_destroy(this);
#endif
}


/** \brief Hook called by Object::Destroy before an audio player is about to be destroyed */

bool CAudioPlayer_PreDestroy(void *self)
{
    audio_perf_call(1);
#ifdef USE_OUTPUTMIXEXT
    CAudioPlayer *this = (CAudioPlayer *) self;
    // Safe to proceed immediately if a track has not yet been assigned
    Track *track = this->mTrack;
    if (NULL == track)
        return true;
    CAudioPlayer *audioPlayer = track->mAudioPlayer;
    if (NULL == audioPlayer)
        return true;
    assert(audioPlayer == this);
    // Request the mixer thread to unlink this audio player's track
    this->mDestroyRequested = true;
    struct timespec deadline;
    if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
        _log_print(3, "[AUDIO] CAudioPlayer_PreDestroy clock_gettime failed");
        return false;
    }
    deadline.tv_nsec += 100000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        ++deadline.tv_sec;
        deadline.tv_nsec -= 1000000000L;
    }
    while (this->mDestroyRequested) {
        if (!zombie_opensles_backend_running())
            break;
        IObject *object = (IObject *)self;
        uint64_t wait_start = sceKernelGetProcessTimeWide();
        int wait_result = pthread_cond_timedwait(&object->mCond,
                                                 &object->mMutex, &deadline);
        audio_perf_wait(1, (unsigned)(sceKernelGetProcessTimeWide()-wait_start), wait_result == ETIMEDOUT);
        if (wait_result == ETIMEDOUT)
            break;
        if (wait_result != 0 && wait_result != EINTR) {
            _log_print(3, "[AUDIO] CAudioPlayer_PreDestroy wait failed: %d",
                       wait_result);
            return false;
        }
    }
    if (this->mDestroyRequested) {
        if (zombie_opensles_backend_running()) {
#ifdef ZOMBIE_Debug_AUDIO
            static unsigned destroy_timeouts;
            unsigned count = destroy_timeouts++;
            if (count < 4 || (count % 128) == 0)
                AUDIO_WARN( "[AUDIO] player destroy timed out while mixer active (count=%u)", count + 1);
#endif
            // Keep the player alive: the mixer may still be reading its track.
            return false;
        }

        // The playback thread has exited, so no mixer can read this track.
        COutputMix *outputMix = CAudioPlayer_GetOutputMix(this);
        IOutputMixExt *mix = &outputMix->mOutputMixExt;
        unsigned index = (unsigned)(track - mix->mTracks);
        if (index >= MAX_TRACK)
            return false;
        object_lock_exclusive(&outputMix->mObject);
        if (track->mAudioPlayer == this && this->mTrack == track) {
            track->mAudioPlayer = NULL;
            track->mReader = NULL;
            track->mAvail = 0;
            mix->mActiveMask &= ~(1u << index);
            this->mTrack = NULL;
            this->mDestroyRequested = false;
            AUDIO_WARN( "[AUDIO] detached player track %u after playback thread exit", index);
        }
        object_unlock_exclusive(&outputMix->mObject);
        if (this->mDestroyRequested)
            return false;
    }
    // Mixer thread has acknowledged the request
#endif
    return true;
}


/** \brief Given an audio player, return its data sink, which is guaranteed to be a non-NULL output
 *  mix.  This function is used by effect send.
 */

COutputMix *CAudioPlayer_GetOutputMix(CAudioPlayer *audioPlayer)
{
    assert(NULL != audioPlayer);
    assert(SL_DATALOCATOR_OUTPUTMIX == audioPlayer->mDataSink.mLocator.mLocatorType);
    SLObjectItf outputMix = audioPlayer->mDataSink.mLocator.mOutputMix.outputMix;
    assert(NULL != outputMix);
    return (COutputMix *) outputMix;
}
