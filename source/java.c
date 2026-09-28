/* Keep the proven JNI surface in java_base.inc, then extend it with the exact
 * Activity.getPreferences / RegistryEnumerator contract recovered from classes4.dex. */
#define fjni_resolve_field base_fjni_resolve_field
#define fjni_borrowed_object base_fjni_borrowed_object
#define fjni_object_class base_fjni_object_class
#define fjni_register_natives base_fjni_register_natives
#define fjni_unregister_natives base_fjni_unregister_natives
#define fjni_resolve_method base_fjni_resolve_method
#define nameToMethodId base_nameToMethodId
#define methodsBoolean base_methodsBoolean
#define methodsByte base_methodsByte
#define methodsChar base_methodsChar
#define methodsDouble base_methodsDouble
#define methodsFloat base_methodsFloat
#define methodsInt base_methodsInt
#define methodsLong base_methodsLong
#define methodsObject base_methodsObject
#define methodsShort base_methodsShort
#define methodsVoid base_methodsVoid
#define nameToFieldId base_nameToFieldId
#define fieldsBoolean base_fieldsBoolean
#define fieldsByte base_fieldsByte
#define fieldsChar base_fieldsChar
#define fieldsDouble base_fieldsDouble
#define fieldsFloat base_fieldsFloat
#define fieldsInt base_fieldsInt
#define fieldsObject base_fieldsObject
#define fieldsLong base_fieldsLong
#define fieldsShort base_fieldsShort
#define nameToMethodId_size base_nameToMethodId_size
#define methodsBoolean_size base_methodsBoolean_size
#define methodsByte_size base_methodsByte_size
#define methodsChar_size base_methodsChar_size
#define methodsDouble_size base_methodsDouble_size
#define methodsFloat_size base_methodsFloat_size
#define methodsInt_size base_methodsInt_size
#define methodsLong_size base_methodsLong_size
#define methodsObject_size base_methodsObject_size
#define methodsShort_size base_methodsShort_size
#define methodsVoid_size base_methodsVoid_size
#define nameToFieldId_size base_nameToFieldId_size
#define fieldsBoolean_size base_fieldsBoolean_size
#define fieldsByte_size base_fieldsByte_size
#define fieldsChar_size base_fieldsChar_size
#define fieldsDouble_size base_fieldsDouble_size
#define fieldsFloat_size base_fieldsFloat_size
#define fieldsInt_size base_fieldsInt_size
#define fieldsObject_size base_fieldsObject_size
#define fieldsLong_size base_fieldsLong_size
#define fieldsShort_size base_fieldsShort_size
#include "java_base.inc"
#undef fjni_resolve_field
#undef fjni_borrowed_object
#undef fjni_object_class
#undef fjni_register_natives
#undef fjni_unregister_natives
#undef fjni_resolve_method
#undef nameToMethodId
#undef methodsBoolean
#undef methodsByte
#undef methodsChar
#undef methodsDouble
#undef methodsFloat
#undef methodsInt
#undef methodsLong
#undef methodsObject
#undef methodsShort
#undef methodsVoid
#undef nameToFieldId
#undef fieldsBoolean
#undef fieldsByte
#undef fieldsChar
#undef fieldsDouble
#undef fieldsFloat
#undef fieldsInt
#undef fieldsObject
#undef fieldsLong
#undef fieldsShort
#undef nameToMethodId_size
#undef methodsBoolean_size
#undef methodsByte_size
#undef methodsChar_size
#undef methodsDouble_size
#undef methodsFloat_size
#undef methodsInt_size
#undef methodsLong_size
#undef methodsObject_size
#undef methodsShort_size
#undef methodsVoid_size
#undef nameToFieldId_size
#undef fieldsBoolean_size
#undef fieldsByte_size
#undef fieldsChar_size
#undef fieldsDouble_size
#undef fieldsFloat_size
#undef fieldsInt_size
#undef fieldsObject_size
#undef fieldsLong_size
#undef fieldsShort_size

#include "preferences_jni.inc"

int fjni_resolve_field(jclass c,const char*n,const char*s,jfieldID*r){return base_fjni_resolve_field(c,n,s,r);}
int fjni_borrowed_object(jobject o){return o==(jobject)&prefs_object||o==(jobject)&prefs_editor_object||base_fjni_borrowed_object(o);}
jclass fjni_object_class(jobject o){if(o==(jobject)&prefs_object)return jni->FindClass(&jni,PREFS_CLASS);if(o==(jobject)&prefs_editor_object)return jni->FindClass(&jni,PREFS_EDITOR_CLASS);return base_fjni_object_class(o);}

jint fjni_register_natives(jclass c,const JNINativeMethod*m,jint n){
 if(class_matches(c,REGISTRY_ENUM_CLASS)){
  if(n<0||(n&&!m))return JNI_ERR;
  for(jint i=0;i<n;i++)if(strcmp(m[i].name,"onKey")||strcmp(m[i].signature,"(Ljava/lang/String;)V")||!m[i].fnPtr)return JNI_ERR;
  for(jint i=0;i<n;i++)registry_key_callback=(RegistryKeyCallback)m[i].fnPtr;
#ifdef ZOMBIE_DEBUG_BUILD
  l_perf("[SAVE] RegistryEnumerator.onKey registered");
#endif
  return JNI_OK;
 }
 return base_fjni_register_natives(c,m,n);
}
void fjni_unregister_natives(jclass c){if(class_matches(c,REGISTRY_ENUM_CLASS))registry_key_callback=NULL;else base_fjni_unregister_natives(c);}

int fjni_resolve_method(jclass c,const char*n,const char*s,jboolean st,jmethodID*r){
 if(c==(jclass)0x42424242&&!st&&!strcmp(n,"getPreferences")&&!strcmp(s,"(I)Landroid/content/SharedPreferences;")){*r=(jmethodID)(uintptr_t)METHOD_PREFS_ACTIVITY;SAVE_TRACE("[SAVE] resolve Activity.getPreferences(I) receiver=0x42424242");return 1;}
 if(class_matches(c,PREFS_CLASS)&&!st){
  if(!strcmp(n,"getString")&&!strcmp(s,"(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;")){*r=(jmethodID)(uintptr_t)METHOD_PREFS_GET;return 1;}
  if(!strcmp(n,"contains")&&!strcmp(s,"(Ljava/lang/String;)Z")){*r=(jmethodID)(uintptr_t)METHOD_PREFS_CONTAINS;return 1;}
  if(!strcmp(n,"edit")&&!strcmp(s,"()Landroid/content/SharedPreferences$Editor;")){*r=(jmethodID)(uintptr_t)METHOD_PREFS_EDIT;return 1;}
  *r=NULL;return 1;
 }
 if(class_matches(c,PREFS_EDITOR_CLASS)&&!st){
  if(!strcmp(n,"putString")&&!strcmp(s,"(Ljava/lang/String;Ljava/lang/String;)Landroid/content/SharedPreferences$Editor;")){*r=(jmethodID)(uintptr_t)METHOD_PREFS_PUT;return 1;}
  if(!strcmp(n,"remove")&&!strcmp(s,"(Ljava/lang/String;)Landroid/content/SharedPreferences$Editor;")){*r=(jmethodID)(uintptr_t)METHOD_PREFS_REMOVE;return 1;}
  if(!strcmp(n,"clear")&&!strcmp(s,"()Landroid/content/SharedPreferences$Editor;")){*r=(jmethodID)(uintptr_t)METHOD_PREFS_CLEAR;return 1;}
  if(!strcmp(n,"commit")&&!strcmp(s,"()Z")){*r=(jmethodID)(uintptr_t)METHOD_PREFS_COMMIT;return 1;}
  if(!strcmp(n,"apply")&&!strcmp(s,"()V")){*r=(jmethodID)(uintptr_t)METHOD_PREFS_APPLY;return 1;}
  *r=NULL;return 1;
 }
 if(class_matches(c,REGISTRY_ENUM_CLASS)){if(st&&!strcmp(n,"enumerateKeys")&&!strcmp(s,"(Landroid/app/Activity;)V")){*r=(jmethodID)(uintptr_t)METHOD_PREFS_ENUM;return 1;}*r=NULL;return 1;}
 return base_fjni_resolve_method(c,n,s,st,r);
}

/* Final dispatch tables: proven base entries plus SharedPreferences. */
NameToMethodID nameToMethodId[]={
 {METHOD_GET_PACKAGE_NAME,"getPackageName",METHOD_TYPE_OBJECT},{METHOD_GET_VERSION,"getVersion",METHOD_TYPE_OBJECT},{METHOD_GET_VERSION_CODE,"getVersionCode",METHOD_TYPE_INT},{METHOD_GET_CLASS_LOADER,"getClassLoader",METHOD_TYPE_OBJECT},{METHOD_LOAD_CLASS,"loadClass",METHOD_TYPE_OBJECT},{METHOD_QUIT,"quit",METHOD_TYPE_VOID},{METHOD_GET_CONTENT_RESOLVER,"getContentResolver",METHOD_TYPE_OBJECT},{METHOD_SECURE_GET_STRING,"getString",METHOD_TYPE_OBJECT}};
MethodsBoolean methodsBoolean[]={{METHOD_PREFS_CONTAINS,prefs_contains},{METHOD_PREFS_COMMIT,prefs_commit}};
MethodsByte methodsByte[]={}; MethodsChar methodsChar[]={}; MethodsDouble methodsDouble[]={}; MethodsFloat methodsFloat[]={};
MethodsInt methodsInt[]={{METHOD_GET_VERSION_CODE,getVersionCode},{METHOD_INPUT_DEVICE_GET_ID,inputGetId},{METHOD_INPUT_DEVICE_GET_SOURCES,inputGetSources},{METHOD_INPUT_HELPER_GET_SOURCES,inputGetSources}};
MethodsLong methodsLong[]={};
MethodsObject methodsObject[]={{METHOD_DISPLAY_METRICS_INIT,metricsInit},{METHOD_GET_WINDOW_MANAGER,getWindowManager},{METHOD_GET_DEFAULT_DISPLAY,getDefaultDisplay},{METHOD_GET_PACKAGE_NAME,getPackageName},{METHOD_GET_VERSION,getVersion},{METHOD_GET_CLASS_LOADER,getClassLoader},{METHOD_LOAD_CLASS,loadClass},{METHOD_GET_CONTENT_RESOLVER,getContentResolver},{METHOD_SECURE_GET_STRING,secureGetString},{METHOD_INPUT_DEVICE_GET_DEVICE,inputGetDevice},{METHOD_INPUT_DEVICE_GET_DEVICE_IDS,inputGetDeviceIds},{METHOD_PREFS_ACTIVITY,activity_get_preferences},{METHOD_PREFS_GET,prefs_get_string},{METHOD_PREFS_EDIT,prefs_edit},{METHOD_PREFS_PUT,prefs_put_string},{METHOD_PREFS_REMOVE,prefs_remove},{METHOD_PREFS_CLEAR,prefs_clear}};
MethodsShort methodsShort[]={};
MethodsVoid methodsVoid[]={{METHOD_DISPLAY_GET_METRICS,displayGetMetrics},{METHOD_QUIT,quit},{METHOD_INPUT_HELPER_GET_RANGES,inputGetMotionRanges},{METHOD_PREFS_APPLY,prefs_apply},{METHOD_PREFS_ENUM,registry_enumerate}};

NameToFieldID nameToFieldId[]={{1100,"xdpi",FIELD_TYPE_FLOAT},{1101,"ydpi",FIELD_TYPE_FLOAT},{FIELD_WINDOW_SERVICE,"WINDOW_SERVICE",FIELD_TYPE_OBJECT},{FIELD_SDK_INT,"SDK_INT",FIELD_TYPE_INT}};
FieldsBoolean fieldsBoolean[]={}; FieldsByte fieldsByte[]={}; FieldsChar fieldsChar[]={}; FieldsDouble fieldsDouble[]={};
FieldsFloat fieldsFloat[]={{1100,220.f},{1101,220.f}}; FieldsInt fieldsInt[]={{FIELD_SDK_INT,SDK_INT}}; FieldsObject fieldsObject[]={{FIELD_WINDOW_SERVICE,WINDOW_SERVICE}}; FieldsLong fieldsLong[]={}; FieldsShort fieldsShort[]={};
__FALSOJNI_IMPL_CONTAINER_SIZES
