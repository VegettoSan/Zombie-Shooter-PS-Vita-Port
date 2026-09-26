#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "psp2/kernel/threadmgr.h"
enum {
 SCE_CTRL_SELECT=1, SCE_CTRL_L3=2, SCE_CTRL_R3=4, SCE_CTRL_START=8,
 SCE_CTRL_UP=0x10, SCE_CTRL_RIGHT=0x20, SCE_CTRL_DOWN=0x40, SCE_CTRL_LEFT=0x80,
 SCE_CTRL_L2=0x100, SCE_CTRL_R2=0x200, SCE_CTRL_L1=0x400, SCE_CTRL_R1=0x800,
 SCE_CTRL_TRIANGLE=0x1000, SCE_CTRL_CIRCLE=0x2000, SCE_CTRL_CROSS=0x4000, SCE_CTRL_SQUARE=0x8000,
 SCE_CTRL_TYPE_PHY=1, SCE_CTRL_TYPE_VIRT=2, SCE_CTRL_TYPE_DS3=4, SCE_CTRL_TYPE_DS4=8,
 SCE_CTRL_MODE_ANALOG_WIDE=2
};
typedef struct { uint64_t timeStamp; unsigned buttons; uint8_t lx,ly,rx,ry,up,right,down,left,lt,rt,l1,r1,triangle,circle,cross,square,reserved[4]; } SceCtrlData;
typedef struct { uint8_t port[5],unk[11]; } SceCtrlPortInfo;
#ifdef __cplusplus
extern "C" {
#endif
int sceCtrlSetSamplingModeExt(int);
int sceCtrlGetControllerPortInfo(SceCtrlPortInfo *);
int sceCtrlPeekBufferPositiveExt2(int,SceCtrlData *,int);
#ifdef __cplusplus
}
#endif
