/* Run the real Release logger with deterministic time and file primitives. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static char output[200000]; static unsigned output_size, mock_syncs, console_calls;
static uint64_t now=100;
/* logger.c normally gets this from settings.c; keep the host regression focused
 * on the real logger implementation without linking the settings parser. */
int setting_log_mode=2;
int sceKernelCreateLwMutex(void *a,const char *b,int c,int d,void *e) { return 0; }
int sceKernelLockLwMutex(void *a,int b,void *c) { return 0; }
int sceKernelUnlockLwMutex(void *a,int b) { return 0; }
int sceKernelDelayThread(unsigned us) { return 0; }
uint64_t sceKernelGetProcessTimeWide(void) { return now; }
int sceIoMkdir(const char *a,int b) { return 0; }
int sceIoGetstat(const char *a,void *b) { return -1; }
int sceIoOpen(const char *a,int b,int c) { return 10; }
int sceIoWrite(int fd,const void *buf,unsigned n) { assert(output_size+n<sizeof(output)); memcpy(output+output_size,buf,n); output_size+=n; output[output_size]=0; return n; }
int sceIoSyncByFd(int fd,int flag) { mock_syncs++; now+=20; return 0; }
int sceClibPrintf(const char *fmt,...) { console_calls++; return 0; }
#define sceClibSnprintf snprintf
#define sceClibVsnprintf vsnprintf
#include "utils/logger.c"
int main(void) {
    l_error("same"); unsigned base=mock_syncs;
    for(int i=1;i<20;++i) l_error("same");
    LoggerStats s={0}; logger_get_stats(&s);
    assert(s.lines==6 && s.suppressed==14 && mock_syncs==base && !console_calls);
    _log_print(LT_WARN,"warning"); _log_print(LT_INFO,"info");
    logger_get_stats(&s); assert(s.lines==6);

    /* Quiet modes flush bytes after the time/count checkpoint, but they must
     * not physically fsync the Vita memory card from the game/render caller. */
    now+=1000000; l_error("unique after second");
    assert(mock_syncs==base);
    assert(strstr(output,"unique after second"));
    assert(strstr(output,"BUILD variant=Release id=test"));
    assert(strstr(output,"repeat_count=8") && strstr(output,"repeat_count=16"));

    base=mock_syncs;
    for(int i=0;i<64;++i) l_error("unique %d",i);
    assert(mock_syncs==base && !console_calls);
    assert(strstr(output,"unique 63"));

    /* Fatal remains crash-durable and console-visible. */
    base=mock_syncs;
    for(int i=0;i<6;++i) l_fatal("fatal same");
    assert(mock_syncs==base+6 && console_calls==6);

    /* mode 2 includes PERF; explicit sync remains available at a diagnostic
     * boundary and is intentionally the only non-fatal way to force fsync. */
    l_perf("logger sample"); base=mock_syncs; logger_force_sync();
    assert(mock_syncs==base+1 && strstr(output,"[PERF] logger sample"));

    /* mode 1 retains safety errors/fatal but skips PERF before formatting,
     * locking and memory-card I/O. */
    setting_log_mode=1;
    unsigned before=output_size;
    l_perf("must not persist");
    assert(output_size==before);
    l_error("mode1 error");
    base=mock_syncs; logger_force_sync();
    assert(mock_syncs==base+1 && strstr(output,"mode1 error"));

    /* mode 0 performs no persistent write at all. Fatal remains visible on the
     * debug console so a completely disabled file logger cannot hide a crash. */
    setting_log_mode=0;
    before=output_size; base=mock_syncs; unsigned console_before=console_calls;
    l_error("mode0 error"); l_perf("mode0 perf"); logger_force_sync();
    assert(output_size==before && mock_syncs==base);
    l_fatal("mode0 fatal");
    assert(output_size==before && mock_syncs==base && console_calls==console_before+1);

    puts("Logger regression passed: quiet flushes avoid gameplay fsync, explicit/fatal sync preserved, modes/repeats/PERF verified");
}
