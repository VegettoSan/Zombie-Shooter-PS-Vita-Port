#ifndef ZOMBIE_PERF_H
#define ZOMBIE_PERF_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { unsigned calls, wait_calls, wait_us, max_us, timeouts; } AudioWaitStats;
typedef struct { AudioWaitStats clear, destroy; unsigned output_calls, output_errors; } AudioPerfStats;
void audio_perf_call(unsigned kind);
void audio_perf_wait(unsigned kind, unsigned us, int timeout);
void audio_perf_output(int result);
void audio_perf_snapshot(AudioPerfStats *out);
void perf_bulk_memset(size_t bytes, uint64_t start);
enum { PERF_ENGINE_GRAPH, PERF_ENGINE_SOFTWARE, PERF_ENGINE_MAP,
       PERF_ENGINE_PRE, PERF_ENGINE_POST, PERF_ENGINE_COLLECTOR, PERF_ENGINE_COUNT };
void perf_engine_phase(unsigned phase, uint64_t start);
void perf_report(void);
#ifdef __cplusplus
}
#endif
#endif
