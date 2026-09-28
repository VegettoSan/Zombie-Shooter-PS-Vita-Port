/* Debug only: original canonical Thumb prologues and exact pointer/integer
 * signatures. Trampolines preserve register/stack ABI and original results;
 * no per-event instruction writes, invented actions or synthetic touches. */
#if defined(ZOMBIE_DEBUG_BUILD) && defined(NDK_PORT)
#include <falso_ndk/android/AInput.h>
#include <errno.h>
static uintptr_t contract_original[8];
static unsigned contract_records;
static unsigned contract_save_records, contract_bind_records, contract_action_records;
static unsigned long (*contract_peek_error)(void);
static void (*contract_error_string)(unsigned long,char *,size_t);
static int (*contract_string_length)(const void *);
static const char *(*contract_string_chars)(const void *);
static int (*contract_action_is_set)(const void *,unsigned);
#define SAVE_TRACE(...) do {if(__atomic_fetch_add(&contract_save_records,1,__ATOMIC_RELAXED)<512)l_perf(__VA_ARGS__);}while(0)
#define CONTRACT_TRACE(...) do {if(__atomic_fetch_add(&contract_records,1,__ATOMIC_RELAXED)<512)l_perf(__VA_ARGS__);}while(0)
static uintptr_t contract_keyboard(void *self,AInputEvent *event) {
    int code=AKeyEvent_getKeyCode(event),action=AKeyEvent_getAction(event);
    uintptr_t result=((uintptr_t(*)(void*,AInputEvent*))contract_original[0])(self,event);
    CONTRACT_TRACE("[INPUT] engine=AndroidKeyboardControl keycode=%d action=%d accepted=%u source=0x%X device=%d",code,action,(unsigned)result,AInputEvent_getSource(event),AInputEvent_getDeviceId(event));
    return result;
}
static uintptr_t contract_binder(void *self,unsigned code,int state) {
    if(code==4 || (code>=19 && code<=23) || (code>=96 && code<=109))
    CONTRACT_TRACE("[INPUT] engine=KeyBinder.setKey keycode=%u state=%d binder=%p",code,state,self);
    return ((uintptr_t(*)(void*,unsigned,int))contract_original[1])(self,code,state);
}
static uintptr_t contract_store(void *self,const void *key,const void *value) {
    SAVE_TRACE("[SAVE] engine=Registry.storeValue self=%p key=%p value=%p caller=%p",self,key,value,__builtin_return_address(0));
    return ((uintptr_t(*)(void*,const void*,const void*))contract_original[2])(self,key,value);
}
static uintptr_t contract_private(void *self,const void *key,const void *value) {
    SAVE_TRACE("[SAVE] engine=RegistryPrivate.setString key=%p value=%p",key,value);
    return ((uintptr_t(*)(void*,const void*,const void*))contract_original[3])(self,key,value);
}
static int contract_encrypt(const void *value,void *out) {
    int result=((int(*)(const void*,void*))contract_original[4])(value,out);
    SAVE_TRACE("[SAVE] engine=crypto.encrypt result=%d",result);
    return result;
}
/* Chipher<1,256>::porcess has four pointer arguments and a bool result.
 * "Invalid key length" checks the vector; "Wrong AES key length" at source
 * line 156 checks EVP_CIPHER_CTX after EVP_CipherInit_ex. Inspect the original
 * error queue without removing an error or replacing any crypto operation. */
static int contract_cipher(void *self,const void *input,void *output,const void *key) {
    const uint32_t *v=key;
    unsigned key_bytes=v[1]-v[0];
    int result=((int(*)(void*,const void*,void*,const void*))contract_original[5])(self,input,output,key);
    static unsigned records;
    if(!result && __atomic_fetch_add(&records,1,__ATOMIC_RELAXED)<16) {
        int saved_errno=errno;
        unsigned long error=contract_peek_error?contract_peek_error():0;
        char description[192]={0};
        if(error && contract_error_string) contract_error_string(error,description,sizeof(description));
        l_perf("[SAVE] AES original_result=%d input_key_bytes=%u openssl_last_error=0x%08lX description=%s",result,key_bytes,error,description);
        logger_force_sync();errno=saved_errno;
    }
    return result;
}
static uintptr_t contract_bind(void *self,unsigned action,const void *keys,unsigned char index) {
    /* Native STRING accessors verified in the canonical SO; no copied layout. */
    if(contract_string_length && contract_string_chars &&
       __atomic_fetch_add(&contract_bind_records,1,__ATOMIC_RELAXED)<256) {
        int length=contract_string_length(keys);
        const char *chars=contract_string_chars(keys);
        l_perf("[INPUT] binding binder=%p action=%u slot=%u combination=%.*s",self,action,index,length<128?length:128,chars?chars:"");
    }
    return ((uintptr_t(*)(void*,unsigned,const void*,unsigned char))contract_original[6])(self,action,keys,index);
}
static uintptr_t contract_action(void *self,unsigned action,bool state) {
    int before=-1;
    if(contract_action_is_set && __atomic_load_n(&contract_action_records,__ATOMIC_RELAXED)<512)
        before=contract_action_is_set(self,action)!=0;
    uintptr_t result=((uintptr_t(*)(void*,unsigned,bool))contract_original[7])(self,action,state);
    if(before>=0 && before!=(int)state &&
       __atomic_fetch_add(&contract_action_records,1,__ATOMIC_RELAXED)<512)
        l_perf("[INPUT] ControlStates.set states=%p action=%u state=%u caller=%p",self,action,(unsigned)state,__builtin_return_address(0));
    return result;
}
static void install_contract_traces(void) {
    contract_peek_error=(void*)so_symbol(&so_mod,"ERR_peek_last_error");
    contract_error_string=(void*)so_symbol(&so_mod,"ERR_error_string_n");
    contract_string_length=(void*)so_symbol(&so_mod,"_ZNK6STRING6LengthEv");
    contract_string_chars=(void*)so_symbol(&so_mod,"_ZNK6STRING5c_strEv");
    contract_action_is_set=(void*)so_symbol(&so_mod,"_ZNK5input13ControlStates5isSetENS_12ActionStates4TypeE");
    static const struct {const char *symbol;unsigned offset;uint32_t code[2];uintptr_t wrapper;} rows[]={
        {"_ZN5input22AndroidKeyboardControl11handleEventEP11AInputEvent",0x3d0774,{0xaf03b5f0,0xbd04f84d},(uintptr_t)contract_keyboard},
        {"_ZN5input9KeyBinder6setKeyEjNS_3Key5StateE",0x424520,{0xaf03b5f0,0x0f00e92d},(uintptr_t)contract_binder},
        {"_ZN4core8Registry10storeValueERK6STRINGS3_",0x3eb08c,{0xaf03b5f0,0x8d04f84d},(uintptr_t)contract_store},
        {"_ZN4core15RegistryPrivate9setStringERK6STRINGS3_",0x3f4e2c,{0xaf03b5f0,0x0b00e92d},(uintptr_t)contract_private},
        {"_ZN4core6crypto7encryptERK6STRINGRS1_",0x3f5be4,{0xaf03b5f0,0x0f00e92d},(uintptr_t)contract_encrypt},
        {"_ZN4core6crypto7ChipherILi1ELi256EE7porcessERKNSt6__ndk16vectorIhNS3_9allocatorIhEEEERS7_S9_",0x3f7440,{0xaf03b5f0,0x0f00e92d},(uintptr_t)contract_cipher},
        {"_ZN5input9KeyBinder8bindKeysENS_12ActionStates4TypeERK6STRINGh",0x424668,{0xaf03b5f0,0x0f00e92d},(uintptr_t)contract_bind},
        {"_ZN5input13ControlStates3setENS_12ActionStates4TypeEb",0x41faa4,{0xaf03b5f0,0x0f00e92d},(uintptr_t)contract_action}
    };
    for(unsigned i=0;i<sizeof(rows)/sizeof(rows[0]);i++) {
        uintptr_t address=so_symbol(&so_mod,rows[i].symbol),entry=address&~(uintptr_t)1;
        uintptr_t arena=(so_mod.patch_head+3)&~(uintptr_t)3;
        uint32_t code[4];
        if(!(address&1) || entry!=so_mod.load_addr+rows[i].offset ||
            arena<so_mod.patch_base || arena>so_mod.patch_base+so_mod.patch_size ||
            so_mod.patch_base+so_mod.patch_size-arena<sizeof(code) ||
            !engine_probe_trampoline(code,(void*)entry,rows[i].code,(uint32_t)(entry+8))) {
            l_error("[PATCH] contract trace guard rejected %s",rows[i].symbol);continue;
        }
        sceClibMemcpy((void*)arena,code,sizeof(code));
        contract_original[i]=arena|1;so_mod.patch_head=arena+sizeof(code);
        hook_addr(address,rows[i].wrapper);
        kuKernelFlushCaches((void*)arena,sizeof(code));kuKernelFlushCaches((void*)entry,8);
        l_perf("[PATCH] contract trace installed %s so+0x%X",rows[i].symbol,rows[i].offset);
    }
}
#else
static void install_contract_traces(void) {}
#endif
