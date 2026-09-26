#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include <cerrno>
#include <cstring>
#include "shim/fndk_controls.h"
#include "android/keycodes.h"
#include "utils/settings.h"
extern void pollPad();
extern void pollTouch();
static SceCtrlData pad;
static SceTouchData touches[2];
static SceCtrlPortInfo ports = {{SCE_CTRL_TYPE_PHY}, {}};
static int read_ok=1, last_port=-1, axis_traces=0, button_traces=0;
extern "C" {
int setting_vita_shooter;
int sceKernelCreateLwMutex(SceKernelLwMutexWork*,const char*,int,int,void*) { return 0; }
int sceKernelLockLwMutex(SceKernelLwMutexWork*,int,void*) { return 0; }
int sceKernelUnlockLwMutex(SceKernelLwMutexWork*,int) { return 0; }
int sceKernelCreateThread(const char*,int(*)(SceSize,void*),int,int,int,int,void*) { return 1; }
int sceKernelStartThread(int,int,void*) { return 0; }
int sceKernelDelayThread(int) { return 0; }
uint64_t sceKernelGetProcessTimeWide() { return 0; }
int sceCtrlSetSamplingModeExt(int) { return 0; }
int sceCtrlGetControllerPortInfo(SceCtrlPortInfo *p) { *p=ports; return 0; }
int sceCtrlPeekBufferPositiveExt2(int port,SceCtrlData *p,int) { last_port=port; if (read_ok) *p=pad; return read_ok; }
int sceTouchSetSamplingState(int,int) { return 0; }
int sceTouchGetPanelInfo(int,SceTouchPanelInfo*p) { *p={0,1920,0,1088}; return 0; }
int sceTouchPeek(int port,SceTouchData*p,int) { *p=touches[port]; return 1; }
int fndk_eventfd(unsigned,int) { return 1; }
ssize_t fndk_read(int,void*,size_t) { errno=EAGAIN; return -1; }
ssize_t fndk_write(int,const void*,size_t count) { return count; }
void ALOGD(const char*fmt,...) { if(strstr(fmt,"[INPUT] axis requested="))axis_traces++; if(strstr(fmt,"[INPUT] button "))button_traces++; }
void ALOGW(const char*,...) {}
void ALOGE(const char*,...) {}
}
static AInputQueue *queue;
static std::vector<inputEvent> drain() {
 std::vector<inputEvent> events; AInputEvent *event;
 while(AInputQueue_getEvent(queue,&event)==0) {
   events.push_back(*reinterpret_cast<inputEvent*>(event));
   AInputQueue_finishEvent(queue,event,1);
 }
 assert(AInputQueue_hasEvents(queue)==0);
 return events;
}
static void tick() { pollPad(); }
static void neutral() { pad={}; pad.lx=pad.ly=pad.rx=pad.ry=128; touches[1]={}; tick(); drain(); }
static const inputEvent &key(const std::vector<inputEvent>&es,int code,int action) {
 for(const auto&e:es) if(e.type==AINPUT_EVENT_TYPE_KEY && e.keycode==code && e.action==action) return e;
 assert(false); return es[0];
}
static const inputEvent &motion(const std::vector<inputEvent>&es) {
 for(const auto&e:es) if(e.type==AINPUT_EVENT_TYPE_MOTION) return e;
 assert(false); return es[0];
}
static float axis(const inputEvent&e,int a) { return AMotionEvent_getAxisValue(reinterpret_cast<const AInputEvent*>(&e),a,0); }
int main() {
 queue=AInputQueue_create(); neutral();
 const unsigned buttons[]={SCE_CTRL_CROSS,SCE_CTRL_CIRCLE,SCE_CTRL_SQUARE,SCE_CTRL_TRIANGLE,SCE_CTRL_START,SCE_CTRL_SELECT,SCE_CTRL_L3,SCE_CTRL_R3};
 const int codes[]={AKEYCODE_BUTTON_A,AKEYCODE_BUTTON_B,AKEYCODE_BUTTON_X,AKEYCODE_BUTTON_Y,AKEYCODE_BUTTON_START,AKEYCODE_BUTTON_SELECT,AKEYCODE_BUTTON_THUMBL,AKEYCODE_BUTTON_THUMBR};
 for(unsigned i=0;i<8;i++) {
  pad.buttons=buttons[i]; tick(); auto es=drain(); const auto&e=key(es,codes[i],AKEY_EVENT_ACTION_DOWN);
  auto *aie=reinterpret_cast<const AInputEvent*>(&e);
  assert(AInputEvent_getDeviceId(aie)==FNDK_GAMEPAD_DEVICE_ID && AInputEvent_getSource(aie)==AINPUT_SOURCE_GAMEPAD);
  assert(AKeyEvent_getRepeatCount(aie)==0 && AKeyEvent_getScanCode(aie)==0);
  tick(); assert(drain().empty());
  pad.buttons=0; tick(); es=drain(); key(es,codes[i],AKEY_EVENT_ACTION_UP);
 }
 pad.lx=0; pad.ly=255; pad.rx=255; pad.ry=0; tick(); auto es=drain(); auto e=motion(es);
 assert(AInputEvent_getDeviceId(reinterpret_cast<AInputEvent*>(&e))==FNDK_GAMEPAD_DEVICE_ID);
 assert(e.device_id!=e.source && e.source==AINPUT_SOURCE_JOYSTICK);
 assert(axis(e,AMOTION_EVENT_AXIS_X)==-1 && axis(e,AMOTION_EVENT_AXIS_Y)==1 && axis(e,AMOTION_EVENT_AXIS_Z)==1 && axis(e,AMOTION_EVENT_AXIS_RZ)==-1);
 pad.lx=pad.ly=pad.rx=pad.ry=128; tick(); es=drain(); e=motion(es); assert(axis(e,AMOTION_EVENT_AXIS_X)==0 && axis(e,AMOTION_EVENT_AXIS_Z)==0);
 const unsigned dirs[]={SCE_CTRL_LEFT,SCE_CTRL_RIGHT,SCE_CTRL_UP,SCE_CTRL_DOWN};
 const int dirkeys[]={AKEYCODE_DPAD_LEFT,AKEYCODE_DPAD_RIGHT,AKEYCODE_DPAD_UP,AKEYCODE_DPAD_DOWN};
 for(unsigned i=0;i<4;i++) {
  neutral(); pad.buttons=dirs[i]; tick(); es=drain(); key(es,dirkeys[i],AKEY_EVENT_ACTION_DOWN); e=motion(es);
  assert(axis(e,AMOTION_EVENT_AXIS_HAT_X)==(i==0?-1:i==1?1:0));
  assert(axis(e,AMOTION_EVENT_AXIS_HAT_Y)==(i==2?-1:i==3?1:0));
 }
 neutral(); pad.buttons=SCE_CTRL_LEFT|SCE_CTRL_RIGHT|SCE_CTRL_UP|SCE_CTRL_DOWN; tick(); es=drain(); assert(es.size()==4); // opposite directions cancel HAT at center
 neutral(); pad.buttons=SCE_CTRL_L2; tick(); es=drain(); key(es,AKEYCODE_BUTTON_L2,0); e=motion(es);
 assert(axis(e,AMOTION_EVENT_AXIS_LTRIGGER)==1 && axis(e,AMOTION_EVENT_AXIS_BRAKE)==1);
 neutral(); pad.buttons=SCE_CTRL_R2; tick(); es=drain(); e=motion(es); assert(axis(e,AMOTION_EVENT_AXIS_RTRIGGER)==1 && axis(e,AMOTION_EVENT_AXIS_GAS)==1);
 neutral(); setting_vita_shooter=1; pad.buttons=SCE_CTRL_L1|SCE_CTRL_R1; tick(); es=drain(); assert(es.size()==3);
 key(es,AKEYCODE_BUTTON_L2,0); key(es,AKEYCODE_BUTTON_R2,0); e=motion(es); assert(axis(e,AMOTION_EVENT_AXIS_BRAKE)==1 && axis(e,AMOTION_EVENT_AXIS_GAS)==1);
 // Rear secondary actions coexist with physical triggers, including held chords.
 touches[1].reportNum=2; touches[1].report[0]={0,500,200}; touches[1].report[1]={1,1400,200}; tick(); es=drain(); assert(es.size()==2);
 key(es,AKEYCODE_BUTTON_L1,0); key(es,AKEYCODE_BUTTON_R1,0); tick(); assert(drain().empty());
 touches[1]={}; tick(); es=drain(); key(es,AKEYCODE_BUTTON_L1,1); key(es,AKEYCODE_BUTTON_R1,1);
 neutral(); setting_vita_shooter=0; touches[1].reportNum=1; touches[1].report[0]={0,500,200}; tick(); es=drain(); key(es,AKEYCODE_BUTTON_L2,0); assert(axis(motion(es),AMOTION_EVENT_AXIS_LTRIGGER)==1); neutral();
 // Actual port detection and analog pressures, even with Shooter enabled.
 setting_vita_shooter=1; ports.port[1]=SCE_CTRL_TYPE_DS4;
 for(int i=0;i<61;i++){tick();drain();} assert(last_port==1);
 pad.buttons=SCE_CTRL_L1|SCE_CTRL_R1|SCE_CTRL_L2|SCE_CTRL_R2|SCE_CTRL_L3|SCE_CTRL_R3; pad.lt=64; pad.rt=192;
 touches[1].reportNum=1; touches[1].report[0]={0,500,200}; tick(); es=drain(); assert(es.size()==7);
 key(es,AKEYCODE_BUTTON_L1,0); key(es,AKEYCODE_BUTTON_R1,0); key(es,AKEYCODE_BUTTON_THUMBL,0); key(es,AKEYCODE_BUTTON_THUMBR,0);
 e=motion(es); assert(fabs(axis(e,AMOTION_EVENT_AXIS_LTRIGGER)-64.f/255)<1e-6 && fabs(axis(e,AMOTION_EVENT_AXIS_RTRIGGER)-192.f/255)<1e-6);
 read_ok=0; tick(); es=drain(); assert(es.size()==7); assert(axis(motion(es),AMOTION_EVENT_AXIS_LTRIGGER)==0); read_ok=1;
 neutral(); tick(); assert(drain().empty());
 // Front touchscreen keeps its separate identity and slot lifecycle.
 touches[0].reportNum=1; touches[0].report[0]={127,960,544}; pollTouch(); es=drain(); e=motion(es);
 assert(e.device_id==FNDK_TOUCH_DEVICE_ID && e.device_id!=FNDK_GAMEPAD_DEVICE_ID && e.source==AINPUT_SOURCE_TOUCHSCREEN);
 assert(e.motion_action==AMOTION_EVENT_ACTION_DOWN && e.motion_ptridx[0]==0);
 pollTouch(); assert(drain().empty()); touches[0]={}; pollTouch(); es=drain(); assert(motion(es).motion_action==AMOTION_EVENT_ACTION_UP);
 // Stable poll state cannot continuously grow the real input queue.
 for(int i=0;i<1000;i++) {tick();pollTouch();} assert(AInputQueue_hasEvents(queue)==0);
#ifdef ZOMBIE_DEBUG_BUILD
 assert(axis_traces==10 && button_traces>0);
#else
 assert(axis_traces==0 && button_traces==0);
#endif
 puts("Gamepad regression passed: identity, buttons, sticks, HAT, aliases, profiles, external port, disconnect, touch and queue");
}
