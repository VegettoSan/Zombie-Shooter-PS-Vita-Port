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

/** \file SDL.c SDL platform implementation */

#include "sles_allinclusive.h"
#include <vitasdk.h>

#include "utils/logger.h"
#include "utils/perf.h"
#if defined(ZOMBIE_Debug_AUDIO)
#define AUDIO_WARN(...) _log_print(LT_WARN, __VA_ARGS__)
#else
#define AUDIO_WARN(...) ((void)0)
#endif

/** \brief Called by SDL to fill the next audio output buffer */
static IEngine *slEngine;

static volatile int audio_shutdown_requested;
static volatile int audio_thread_running;
static int audio_port = -1;

int zombie_opensles_backend_running(void) {
	return audio_thread_running;
}

#ifdef HAVE_PTHREAD
static pthread_t audio_thread_handle;
static int audio_thread_valid;
#else
static SceUID audio_thread_handle = -1;
#endif

uint8_t audio_buffers[SndFile_NUMBUFS][SndFile_BUFSIZE];

static void reset_audio_backend_state(void) {
	audio_shutdown_requested = 1;
	audio_thread_running = 0;
	audio_port = -1;
#ifdef HAVE_PTHREAD
	audio_thread_valid = 0;
#else
	audio_thread_handle = -1;
#endif
	slEngine = NULL;
}

static int opensles_output_freq(void) {
	return (&_opensles_user_freq != NULL && _opensles_user_freq > 0)
						 ? _opensles_user_freq
						 : 44100;
}

static void fill_output_buffer(uint8_t *stream, SLuint32 size) {
	sceClibMemset(stream, 0, (size_t)size);

	if (NULL == slEngine) {
		return;
	}

	COutputMix *outputMix = slEngine->mOutputMix;
	if (NULL != outputMix) {
		SLOutputMixExtItf OutputMixExt = &outputMix->mOutputMixExt.mItf;
		IOutputMixExt_FillBuffer(OutputMixExt, stream, size);
	}
}

#ifdef HAVE_PTHREAD
static void *audioThread(void *arg) {
#else
static int audioThread(unsigned int args, void *arg) {
#endif
	(void)arg;
#ifndef HAVE_PTHREAD
	(void)args;
#endif

	int ch = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, SndFile_BUFSIZE / 4,
															 opensles_output_freq(),
															 SCE_AUDIO_OUT_MODE_STEREO);
	if (ch < 0) {
		_log_print(3, "[AUDIO] sceAudioOutOpenPort(BGM, frames=%u, hz=%d) failed: 0x%08X",
			(unsigned)(SndFile_BUFSIZE / 4), opensles_output_freq(), (unsigned)ch);
		SL_LOGE("Unable to open Vita audio port: 0x%x", ch);
#ifdef HAVE_PTHREAD
		return NULL;
#else
		return ch;
#endif
	} else {
		_log_print(1, "[AUDIO] sceAudioOutOpenPort OK port=%d frames=%u hz=%d",
			ch, (unsigned)(SndFile_BUFSIZE / 4), opensles_output_freq());
		SL_LOGI("Opened Vita audio port %d", ch);
	}

	audio_port = ch;
	audio_thread_running = 1;
	int res;

	int vol_stereo[] = {32767, 32767};
	res = sceAudioOutSetVolume(
			ch,
			(SceAudioOutChannelFlag)(SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH),
			vol_stereo);
	if (res < 0) {
		_log_print(3, "[AUDIO] sceAudioOutSetVolume failed: 0x%08X", (unsigned)res);
		SL_LOGE("Unable to set Vita audio volume on port %d: 0x%x", ch, res);
		goto exit_thread;
	}

	int buf_idx = 0;

	while (!audio_shutdown_requested) {
		uint8_t *stream = audio_buffers[buf_idx];
		buf_idx = (buf_idx + 1) % SndFile_NUMBUFS;

		fill_output_buffer(stream, (SLuint32)SndFile_BUFSIZE);
		res = sceAudioOutOutput(ch, stream);
		audio_perf_output(res);
		if (res < 0) {
			_log_print(3, "[AUDIO] sceAudioOutOutput failed: port=%d result=0x%08X",
				ch, (unsigned)res);
			SL_LOGE("Vita audio output failed on port %d: 0x%x", ch, res);
			break;
		}
	}

exit_thread:
	AUDIO_WARN( "[AUDIO] OpenSLES Playback exiting: shutdown=%d last_result=0x%08X",
		audio_shutdown_requested, (unsigned)res);
	if (audio_port == ch) {
		sceAudioOutReleasePort(ch);
		audio_port = -1;
	}
	audio_thread_running = 0;

#ifdef HAVE_PTHREAD
	return NULL;
#else
	return 0;
#endif
}

/** \brief Called during slCreateEngine */

void SDL_open(IEngine *thisEngine)
{
	SDL_close();
	if (NULL == thisEngine) {
		return;
	}
	slEngine = thisEngine;
	audio_shutdown_requested = 0;
	audio_thread_running = 0;
#ifdef HAVE_PTHREAD
	audio_thread_valid = (0 == pthread_create(&audio_thread_handle, NULL, audioThread, NULL));
	if (!audio_thread_valid) {
		SL_LOGE("Unable to create OpenSLES playback thread");
		reset_audio_backend_state();
	}
#else
	audio_thread_handle = sceKernelCreateThread("OpenSLES Playback", &audioThread, 0x10000100,
                                                0x10000, 0, 0, NULL);
	if (audio_thread_handle < 0) {
		_log_print(3, "[AUDIO] sceKernelCreateThread failed: 0x%08X",
			(unsigned)audio_thread_handle);
		SL_LOGE("Unable to create OpenSLES playback thread: 0x%x", audio_thread_handle);
		reset_audio_backend_state();
		return;
	}

	int res = sceKernelStartThread(audio_thread_handle, 0, NULL);
	if (res < 0) {
		_log_print(3, "[AUDIO] sceKernelStartThread failed: 0x%08X", (unsigned)res);
		SL_LOGE("Unable to start OpenSLES playback thread %d: 0x%x", audio_thread_handle, res);
		sceKernelDeleteThread(audio_thread_handle);
		reset_audio_backend_state();
	} else {
		_log_print(1, "[AUDIO] OpenSLES Playback thread started: uid=0x%08X",
			(unsigned)audio_thread_handle);
	}
#endif
}


/** \brief Called during Object::Destroy */

void SDL_close(void)
{
	audio_shutdown_requested = 1;
	slEngine = NULL;
#ifdef HAVE_PTHREAD
	if (audio_thread_valid) {
		pthread_join(audio_thread_handle, NULL);
		audio_thread_valid = 0;
	}
#else
	if (audio_thread_handle >= 0) {
		sceKernelWaitThreadEnd(audio_thread_handle, NULL, NULL);
		sceKernelDeleteThread(audio_thread_handle);
		audio_thread_handle = -1;
	}
#endif
	if (audio_port >= 0) {
		sceAudioOutReleasePort(audio_port);
		audio_port = -1;
	}
	audio_thread_running = 0;
	audio_port = -1;
	slEngine = NULL;
}
