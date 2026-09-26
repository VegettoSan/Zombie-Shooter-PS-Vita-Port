#pragma once
#include <stdint.h>
#include <stddef.h>
typedef unsigned SceSize;
typedef int SceUID;
typedef struct { int unused; } SceKernelLwMutexWork;
#ifdef __cplusplus
extern "C" {
#endif
int sceKernelCreateLwMutex(SceKernelLwMutexWork *, const char *, int, int, void *);
int sceKernelLockLwMutex(SceKernelLwMutexWork *, int, void *);
int sceKernelUnlockLwMutex(SceKernelLwMutexWork *, int);
int sceKernelCreateThread(const char *, int (*)(SceSize,void*), int, int, int, int, void *);
int sceKernelStartThread(int,int,void*);
int sceKernelDelayThread(int);
#ifdef __cplusplus
}
#endif
