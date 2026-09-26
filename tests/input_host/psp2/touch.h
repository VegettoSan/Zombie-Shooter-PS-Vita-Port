#pragma once
#include <stdint.h>
#include "psp2/kernel/threadmgr.h"
enum { SCE_TOUCH_PORT_FRONT=0, SCE_TOUCH_PORT_BACK=1, SCE_TOUCH_SAMPLING_STATE_START=1 };
typedef struct { uint8_t id; uint16_t x,y; } SceTouchReport;
typedef struct { unsigned reportNum; SceTouchReport report[10]; } SceTouchData;
typedef struct { int minAaX,maxAaX,minAaY,maxAaY; } SceTouchPanelInfo;
#ifdef __cplusplus
extern "C" {
#endif
int sceTouchSetSamplingState(int,int);
int sceTouchGetPanelInfo(int,SceTouchPanelInfo*);
int sceTouchPeek(int,SceTouchData*,int);
#ifdef __cplusplus
}
#endif
