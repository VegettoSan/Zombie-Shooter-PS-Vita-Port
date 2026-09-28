/* Run the actual Debug logger. Only mode3 is verbose/synchronous per record. */
#define main release_logger_test_unused
#include "logger_regression.c"
#undef main
int main(void) {
    setting_log_mode=2;
    l_perf("quiet first");unsigned base=mock_syncs;
    assert(console_calls==0 && !strstr(output,"quiet first"));
    for(unsigned i=0;i<20;i++) l_perf("sample %u",i);
    assert(mock_syncs==base && console_calls==0);
    l_debug("hidden JNI");l_warn("hidden warning");
    logger_force_sync();assert(mock_syncs==base+1);
    assert(strstr(output,"quiet first") && strstr(output,"sample 19"));
    assert(!strstr(output,"hidden JNI") && !strstr(output,"hidden warning"));

    /* Quiet Debug errors must be visible in the log without physically syncing
     * the memory card from the gameplay caller. */
    base=mock_syncs;l_error("critical");
    assert(mock_syncs==base && strstr(output,"critical") && console_calls==0);

    /* The one-second checkpoint also drains userspace bytes without fsync. */
    now+=1000000;base=mock_syncs;l_perf("one second");
    assert(mock_syncs==base && strstr(output,"one second"));

    /* Explicit verbose mode keeps its immediate diagnostic behavior. */
    base=mock_syncs;setting_log_mode=3;l_debug("explicit verbose");
    assert(strstr(output,"explicit verbose") && console_calls==1 && mock_syncs==base);

    /* Fatal remains synchronous/durable even after returning to quiet mode. */
    setting_log_mode=2;l_fatal("fatal");
    assert(console_calls==2 && mock_syncs==base+1);
    puts("Quiet Debug logger PASS: PERF/errors flush without gameplay fsync, fatal/explicit sync preserved, mode3 preserved");
}
