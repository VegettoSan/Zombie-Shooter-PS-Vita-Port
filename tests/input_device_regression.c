#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <falso_jni/FalsoJNI.h>
#include <falso_ndk/android/AInput.h>
#define LOG_STUB(name) void name(const char *fi,int li,const char *fn,const char *fmt,...) { (void)fi;(void)li;(void)fn;(void)fmt; }
void _log_print(int type, const char *fmt, ...) {(void)type;(void)fmt;}
LOG_STUB(_fjni_log_info) LOG_STUB(_fjni_log_warn) LOG_STUB(_fjni_log_debug) LOG_STUB(_fjni_log_error)
static int count;
static int registry_count;
static char registry_key[128];
static void axisInfo(JNIEnv *env,jclass clazz,jint axis,jfloat min,jfloat max,jfloat flat) {
 assert(env==&jni && clazz && max==1 && flat==0.1f);
 const int expected[]={0,1,11,14,15,16,17,18,23,22};
 assert(count<10 && axis==expected[count] && min==(count<6?-1.f:0.f)); count++;
}
static void registryKey(JNIEnv *env,jclass clazz,jstring key) {
 assert(env==&jni && clazz && key);
 const char *s=jni->GetStringUTFChars(&jni,key,NULL); assert(s);
 registry_count++;
 snprintf(registry_key,sizeof(registry_key),"%s",s);
 jni->ReleaseStringUTFChars(&jni,key,(char*)s);
}
int main(void) {
 remove("shared_preferences.bin"); remove("shared_preferences.tmp");
 jni_init();
 jclass metrics=jni->FindClass(&jni,"android/util/DisplayMetrics");
 jmethodID init=jni->GetMethodID(&jni,metrics,"<init>","()V"); assert(init);
 assert(!jni->GetMethodID(&jni,metrics,"<init>","(I)V"));
 jobject dm=jni->NewObject(&jni,metrics,init); assert(dm);
 jfieldID x=jni->GetFieldID(&jni,metrics,"xdpi","F"), y=jni->GetFieldID(&jni,metrics,"ydpi","F");
 assert(x && y && x!=y);
 assert(!jni->GetFieldID(&jni,metrics,"xdpi","I"));
 assert(!jni->GetFieldID(&jni,metrics,"widthPixels","I"));
 assert(jni->GetFloatField(&jni,dm,x)==220.f && jni->GetFloatField(&jni,dm,y)==220.f);
 jmethodID wm=jni->GetMethodID(&jni,(jclass)0x42424242,"getWindowManager","()Landroid/view/WindowManager;"); assert(wm);
 jobject window=jni->CallObjectMethod(&jni,(jobject)0x42424242,wm); assert(window);
 jclass wc=jni->GetObjectClass(&jni,window);
 jmethodID gd=jni->GetMethodID(&jni,wc,"getDefaultDisplay","()Landroid/view/Display;"); assert(gd);
 jobject display=jni->CallObjectMethod(&jni,window,gd); assert(display);
 jclass dc=jni->GetObjectClass(&jni,display);
 jmethodID gm=jni->GetMethodID(&jni,dc,"getMetrics","(Landroid/util/DisplayMetrics;)V"); assert(gm);
 jni->CallVoidMethod(&jni,display,gm,dm);
 jni->DeleteGlobalRef(&jni,window); jni->DeleteGlobalRef(&jni,display); jni->DeleteGlobalRef(&jni,dm);
 puts("DisplayMetrics JNI regression passed: exact descriptors, nonzero fields and complete activity/display chain");

 jclass clazz=jni->FindClass(&jni,"android/view/InputDevice");
 jclass helper=jni->FindClass(&jni,"com/sigmateam/sige/InputDeviceHelper");
 jmethodID get=jni->GetStaticMethodID(&jni,clazz,"getDevice","(I)Landroid/view/InputDevice;");
 jmethodID id=jni->GetMethodID(&jni,clazz,"getId","()I");
 jmethodID sources=jni->GetMethodID(&jni,clazz,"getSources","()I");
 assert(get && id && sources && get!=id);
 assert(!jni->GetStaticMethodID(&jni,clazz,"getDevice","()I"));
 assert(!jni->GetMethodID(&jni,clazz,"getDevice","(I)Landroid/view/InputDevice;"));
 jclass other=jni->FindClass(&jni,"example/Other"); assert(!jni->GetStaticMethodID(&jni,other,"getDevice","(I)Landroid/view/InputDevice;"));
 jobject obj=jni->CallStaticObjectMethod(&jni,clazz,get,FNDK_GAMEPAD_DEVICE_ID);
 assert(obj && obj==jni->CallStaticObjectMethod(&jni,clazz,get,FNDK_GAMEPAD_DEVICE_ID));
 assert(!jni->CallStaticObjectMethod(&jni,clazz,get,FNDK_TOUCH_DEVICE_ID));
 assert(!jni->CallStaticObjectMethod(&jni,clazz,get,-1));
 assert(jni->CallIntMethod(&jni,obj,id)==FNDK_GAMEPAD_DEVICE_ID);
 int bits=jni->CallIntMethod(&jni,obj,sources);
 assert((bits&AINPUT_SOURCE_GAMEPAD)==AINPUT_SOURCE_GAMEPAD && (bits&AINPUT_SOURCE_JOYSTICK)==AINPUT_SOURCE_JOYSTICK && (bits&AINPUT_SOURCE_DPAD)==AINPUT_SOURCE_DPAD);
 assert(strcmp((char*)jni->GetObjectClass(&jni,obj),"android/view/InputDevice")==0);
 jni->DeleteGlobalRef(&jni,jni->NewGlobalRef(&jni,obj));
 assert(obj==jni->CallStaticObjectMethod(&jni,clazz,get,1));
 jmethodID ids=jni->GetStaticMethodID(&jni,clazz,"getDeviceIds","()[I");
 jintArray arr=jni->CallStaticObjectMethod(&jni,clazz,ids); assert(jni->GetArrayLength(&jni,arr)==1);
 jint value=0; jni->GetIntArrayRegion(&jni,arr,0,1,&value); assert(value==1); jni->DeleteGlobalRef(&jni,arr);
 jmethodID ranges=jni->GetStaticMethodID(&jni,helper,"getMotionRanges","(Landroid/view/InputDevice;)V");
 jmethodID allSources=jni->GetStaticMethodID(&jni,helper,"getInputSources","()I");
 assert(ranges && jni->CallStaticIntMethod(&jni,helper,allSources)==bits);
 JNINativeMethod bad={"onAxisInfo","(IFF)V",(void*)axisInfo}; assert(jni->RegisterNatives(&jni,helper,&bad,1)==JNI_ERR);
 JNINativeMethod good={"onAxisInfo","(IFFF)V",(void*)axisInfo}; assert(jni->RegisterNatives(&jni,helper,&good,1)==JNI_OK);
 jni->CallStaticVoidMethod(&jni,helper,ranges,NULL); assert(count==0);
 jni->CallStaticVoidMethod(&jni,helper,ranges,obj); assert(count==10);
 jni->UnregisterNatives(&jni,helper); jni->CallStaticVoidMethod(&jni,helper,ranges,obj); assert(count==10);
 puts("InputDevice JNI regression passed: scoped signatures, stable object, metadata, lifetime and native axis callback");

 /* classes4.dex RegistryEnumerator uses Activity.getPreferences(0), then
  * enumerates SharedPreferences.getAll().keySet() and invokes native onKey().
  * The port provides the same observable behavior directly. */
 jobject activity=(jobject)0x42424242;
 jmethodID getprefs=jni->GetMethodID(&jni,(jclass)activity,"getPreferences","(I)Landroid/content/SharedPreferences;"); assert(getprefs);
 jobject prefs=jni->CallObjectMethod(&jni,activity,getprefs,0); assert(prefs);
 jclass pc=jni->GetObjectClass(&jni,prefs); assert(pc && !strcmp((char*)pc,"android/content/SharedPreferences"));
 jmethodID getstr=jni->GetMethodID(&jni,pc,"getString","(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;"); assert(getstr);
 jmethodID contains=jni->GetMethodID(&jni,pc,"contains","(Ljava/lang/String;)Z"); assert(contains);
 jmethodID edit=jni->GetMethodID(&jni,pc,"edit","()Landroid/content/SharedPreferences$Editor;"); assert(edit);
 jstring k=jni->NewStringUTF(&jni,"campaign_progress");
 jstring fallback=jni->NewStringUTF(&jni,"missing");
 jstring missing=jni->CallObjectMethod(&jni,prefs,getstr,k,fallback); assert(missing==fallback);
 assert(jni->CallBooleanMethod(&jni,prefs,contains,k)==JNI_FALSE);
 jobject editor=jni->CallObjectMethod(&jni,prefs,edit); assert(editor);
 jclass ec=jni->GetObjectClass(&jni,editor); assert(ec && !strcmp((char*)ec,"android/content/SharedPreferences$Editor"));
 jmethodID put=jni->GetMethodID(&jni,ec,"putString","(Ljava/lang/String;Ljava/lang/String;)Landroid/content/SharedPreferences$Editor;"); assert(put);
 jmethodID commit=jni->GetMethodID(&jni,ec,"commit","()Z"); assert(commit);
 jmethodID remove_id=jni->GetMethodID(&jni,ec,"remove","(Ljava/lang/String;)Landroid/content/SharedPreferences$Editor;"); assert(remove_id);
 jmethodID clear=jni->GetMethodID(&jni,ec,"clear","()Landroid/content/SharedPreferences$Editor;"); assert(clear);
 jmethodID apply=jni->GetMethodID(&jni,ec,"apply","()V"); assert(apply);
 jstring saved=jni->NewStringUTF(&jni,"level2");
 assert(jni->CallObjectMethod(&jni,editor,put,k,saved)==editor);
 assert(jni->CallBooleanMethod(&jni,editor,commit)==JNI_TRUE);
 assert(jni->CallBooleanMethod(&jni,prefs,contains,k)==JNI_TRUE);
 jstring loaded=jni->CallObjectMethod(&jni,prefs,getstr,k,fallback); assert(loaded && loaded!=fallback);
 const char *loaded_utf=jni->GetStringUTFChars(&jni,loaded,NULL); assert(loaded_utf && !strcmp(loaded_utf,"level2"));
 jni->ReleaseStringUTFChars(&jni,loaded,(char*)loaded_utf);

 jclass reg=jni->FindClass(&jni,"com/sigmateam/sige/RegistryEnumerator");
 jmethodID enumerate=jni->GetStaticMethodID(&jni,reg,"enumerateKeys","(Landroid/app/Activity;)V"); assert(enumerate);
 assert(!jni->GetStaticMethodID(&jni,reg,"enumerateKeys","()V"));
 JNINativeMethod badreg={"onKey","(Ljava/lang/Object;)V",(void*)registryKey}; assert(jni->RegisterNatives(&jni,reg,&badreg,1)==JNI_ERR);
 JNINativeMethod goodreg={"onKey","(Ljava/lang/String;)V",(void*)registryKey}; assert(jni->RegisterNatives(&jni,reg,&goodreg,1)==JNI_OK);
 jni->CallStaticVoidMethod(&jni,reg,enumerate,activity);
 assert(registry_count==1 && !strcmp(registry_key,"campaign_progress"));
 jni->UnregisterNatives(&jni,reg);
 assert(jni->CallObjectMethod(&jni,editor,remove_id,k)==editor);
 jni->CallVoidMethod(&jni,editor,apply);
 assert(jni->CallBooleanMethod(&jni,prefs,contains,k)==JNI_FALSE);
 assert(jni->CallObjectMethod(&jni,editor,put,k,saved)==editor);
 assert(jni->CallObjectMethod(&jni,editor,clear)==editor);
 assert(jni->CallBooleanMethod(&jni,editor,commit)==JNI_TRUE);
 assert(jni->CallBooleanMethod(&jni,prefs,contains,k)==JNI_FALSE);

 jni->DeleteGlobalRef(&jni,k); jni->DeleteGlobalRef(&jni,fallback); jni->DeleteGlobalRef(&jni,saved); jni->DeleteGlobalRef(&jni,loaded);
 remove("shared_preferences.bin"); remove("shared_preferences.tmp");
 puts("SharedPreferences regression passed: exact descriptors, string persistence and RegistryEnumerator.onKey enumeration");
}
