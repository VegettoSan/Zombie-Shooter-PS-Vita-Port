/* Protect PCM readers only while FillBuffer is executing. Never held during
 * sceAudioOutOutput or music mixing. Clear only tries: it must not wait while
 * holding the player mutex, because the mixer may need that mutex. */
#ifndef ZOMBIE_MIXER_GATE_H
#define ZOMBIE_MIXER_GATE_H
void zombie_opensles_mix_begin(void);
void zombie_opensles_mix_end(void);
int zombie_opensles_clear_try_begin(void);
void zombie_opensles_clear_end(void);
#endif
