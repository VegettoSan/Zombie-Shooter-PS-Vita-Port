/* MIT. Original M4A/AAC decoder; worker thread only. */
#ifndef ZOMBIE_MUSIC_DECODER_H
#define ZOMBIE_MUSIC_DECODER_H
#include <stdint.h>
#include <stddef.h>
typedef struct MusicDecoder MusicDecoder;
MusicDecoder *music_decoder_open(const char *path);
int music_decoder_read(MusicDecoder *d,int16_t *stereo,unsigned frames,int loop);
void music_decoder_destroy(MusicDecoder *d);
size_t music_decoder_bytes(const MusicDecoder *d);
#endif
