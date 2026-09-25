# OpenSL ES buffer queue override

The headers and `IBufferQueue.c` are from
[`frangarcj/opensles` commit `e35b063`](https://github.com/frangarcj/opensles/tree/e35b0630ac4c9091db63374d5c111d719887dde9/libopensles),
the source revision selected by VitaSDK's `opensles` package. The files
retain their upstream Apache 2.0 notices.

The port changes only `IBufferQueue_Clear`: the mixer's acknowledgement wait
has a 100 ms limit. On timeout, the clear request remains pending, so the
mixing thread still owns the queued buffers and may clear them when it resumes.
The game thread receives `SL_RESULT_RESOURCE_ERROR` and continues. A bounded
`[AUDIO]` warning records the event in the port log.

CMake compiles this one object and replaces the matching member in a build
directory copy of VitaSDK's `libOpenSLES.a`. The installed SDK is untouched.
