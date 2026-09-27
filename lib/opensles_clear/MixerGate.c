#include <pthread.h>
#include "MixerGate.h"
static pthread_mutex_t mixer_gate = PTHREAD_MUTEX_INITIALIZER;
void zombie_opensles_mix_begin(void) { pthread_mutex_lock(&mixer_gate); }
void zombie_opensles_mix_end(void) { pthread_mutex_unlock(&mixer_gate); }
int zombie_opensles_clear_try_begin(void) { return pthread_mutex_trylock(&mixer_gate)==0; }
void zombie_opensles_clear_end(void) { pthread_mutex_unlock(&mixer_gate); }
