# OpenSL ES Vita overrides

The headers, `IBufferQueue.c`, `CAudioPlayer.c`, and `Vita.c` are from
[`frangarcj/opensles` commit `e35b063`](https://github.com/frangarcj/opensles/tree/e35b0630ac4c9091db63374d5c111d719887dde9/libopensles),
the source revision selected by VitaSDK's `opensles` package. The files
retain their upstream Apache 2.0 notices.

The port changes only `IBufferQueue_Clear`: the mixer's acknowledgement wait
has a 100 ms limit. On timeout, the clear request remains pending, so the
mixing thread still owns the queued buffers and may clear them when it resumes.
The game thread receives `SL_RESULT_RESOURCE_ERROR` and continues. A bounded
`[AUDIO]` warning records the event in the port log.

`CAudioPlayer_PreDestroy` also has a 100 ms acknowledgement limit. If the
playback thread has exited, it detaches the player track under the output mix
lock before destruction. If the playback thread is still active, destruction
is refused to avoid freeing a track it may still read. This can retain a player
until the mixer recovers; the next physical test must establish which path occurs.

`Vita.c` logs playback thread start, audio port open, output errors and thread
exit. CMake replaces these three archive members in a build-directory copy of
VitaSDK's `libOpenSLES.a`; the installed SDK is untouched.
