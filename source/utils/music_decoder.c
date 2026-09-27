/* MIT. Decode original AAC in MP4, from bounded immutable compressed RAM.
 * FFmpeg LGPL libraries remain external SDK dependencies. No disk access or
 * codec work reaches the output thread. No full-length PCM allocation. */
#include "utils/music_decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mem.h>
#define MUSIC_FILE_MAX (3u*1024u*1024u)
#define DECODE_STAGE_FRAMES 8192u
struct MusicDecoder {
    uint8_t *bytes;size_t size,pos;
    AVFormatContext *format;AVIOContext *io;AVCodecContext *codec;
    AVFrame *frame;AVPacket *packet;SwrContext *swr;int stream,draining;
    unsigned stage_pos,stage_frames;
    int16_t stage[DECODE_STAGE_FRAMES*2];
};
static int memory_read(void *opaque,uint8_t *dst,int count) {
    MusicDecoder *d=opaque;size_t n=d->size-d->pos;
    if(!n) return AVERROR_EOF;
    if(n>(size_t)count) n=(size_t)count;
    memcpy(dst,d->bytes+d->pos,n);d->pos+=n;return (int)n;
}
static int64_t memory_seek(void *opaque,int64_t offset,int whence) {
    MusicDecoder *d=opaque;
    if(whence==AVSEEK_SIZE) return (int64_t)d->size;
    whence &= ~AVSEEK_FORCE;
    int64_t base=whence==SEEK_SET?0:whence==SEEK_CUR?(int64_t)d->pos:whence==SEEK_END?(int64_t)d->size:-1;
    if(base<0 || offset < -base || offset > (int64_t)d->size-base) return AVERROR(EINVAL);
    d->pos=(size_t)(base+offset);return (int64_t)d->pos;
}
size_t music_decoder_bytes(const MusicDecoder *d) { return d?d->size+sizeof(*d)+32768:0; }
void music_decoder_destroy(MusicDecoder *d) {
    if(!d) return;
    swr_free(&d->swr);av_packet_free(&d->packet);av_frame_free(&d->frame);
    avcodec_free_context(&d->codec);avformat_close_input(&d->format);
    if(d->io) { av_freep(&d->io->buffer);avio_context_free(&d->io); }
    free(d->bytes);free(d);
}
MusicDecoder *music_decoder_open(const char *path) {
    FILE *f=fopen(path,"rb");if(!f) return NULL;
    if(fseek(f,0,SEEK_END)) { fclose(f);return NULL; }
    long n=ftell(f);if(n<=0 || n>(long)MUSIC_FILE_MAX || fseek(f,0,SEEK_SET)) { fclose(f);return NULL; }
    MusicDecoder *d=calloc(1,sizeof(*d));if(!d) { fclose(f);return NULL; }
    d->size=(size_t)n;d->bytes=malloc(d->size);
    int ok=d->bytes && fread(d->bytes,1,d->size,f)==d->size;fclose(f);
    if(!ok) goto fail;
    uint8_t *io_buffer=av_malloc(32768);if(!io_buffer) goto fail;
    d->io=avio_alloc_context(io_buffer,32768,0,d,memory_read,NULL,memory_seek);
    if(!d->io) { av_free(io_buffer);goto fail; }
    d->format=avformat_alloc_context();if(!d->format) goto fail;
    d->format->pb=d->io;d->format->flags|=AVFMT_FLAG_CUSTOM_IO;
    if(avformat_open_input(&d->format,NULL,NULL,NULL)<0 || avformat_find_stream_info(d->format,NULL)<0) goto fail;
    d->stream=av_find_best_stream(d->format,AVMEDIA_TYPE_AUDIO,-1,-1,NULL,0);if(d->stream<0) goto fail;
    AVCodecParameters *params=d->format->streams[d->stream]->codecpar;
    if(params->codec_id!=AV_CODEC_ID_AAC || params->sample_rate!=44100 || params->ch_layout.nb_channels<1 || params->ch_layout.nb_channels>2) goto fail;
    const AVCodec *codec=avcodec_find_decoder(AV_CODEC_ID_AAC);if(!codec) goto fail;
    d->codec=avcodec_alloc_context3(codec);if(!d->codec) goto fail;
    if(avcodec_parameters_to_context(d->codec,params)<0) goto fail;
    d->codec->thread_count=1;
    if(avcodec_open2(d->codec,codec,NULL)<0) goto fail;
    AVChannelLayout stereo=AV_CHANNEL_LAYOUT_STEREO;
    if(swr_alloc_set_opts2(&d->swr,&stereo,AV_SAMPLE_FMT_S16,44100,&d->codec->ch_layout,d->codec->sample_fmt,d->codec->sample_rate,0,NULL)<0 || swr_init(d->swr)<0) goto fail;
    d->frame=av_frame_alloc();d->packet=av_packet_alloc();if(!d->frame || !d->packet) goto fail;
    return d;
fail:
    music_decoder_destroy(d);return NULL;
}
static int next_stage(MusicDecoder *d,int loop) {
    unsigned guard=0,loops=0;
    while(guard++<4096) {
        int result=avcodec_receive_frame(d->codec,d->frame);
        if(result==0) {
            int n=swr_convert(d->swr,(uint8_t **)(uint8_t *[]){(uint8_t *)d->stage},DECODE_STAGE_FRAMES,
                (const uint8_t **)d->frame->extended_data,d->frame->nb_samples);
            av_frame_unref(d->frame);if(n<0) return -1;
            if(n) { d->stage_pos=0;d->stage_frames=(unsigned)n;return n; }continue;
        }
        if(result==AVERROR_EOF) {
            if(!loop || loops++) return 0;
            AVStream *s=d->format->streams[d->stream];
            int64_t start=s->start_time==AV_NOPTS_VALUE?0:s->start_time;
            if(av_seek_frame(d->format,d->stream,start,AVSEEK_FLAG_BACKWARD)<0) return -1;
            avcodec_flush_buffers(d->codec);swr_close(d->swr);if(swr_init(d->swr)<0) return -1;
            d->draining=0;continue;
        }
        if(result!=AVERROR(EAGAIN)) return -1;
        if(d->draining) return 0;
        int read=av_read_frame(d->format,d->packet);
        if(read<0) { if(read!=AVERROR_EOF) return -1;avcodec_send_packet(d->codec,NULL);d->draining=1;continue; }
        if(d->packet->stream_index==d->stream) result=avcodec_send_packet(d->codec,d->packet);else result=0;
        av_packet_unref(d->packet);if(result<0) return -1;
    }
    return -1;
}
int music_decoder_read(MusicDecoder *d,int16_t *stereo,unsigned frames,int loop) {
    unsigned total=0;
    if(!d || !stereo) return -1;
    while(total<frames) {
        if(d->stage_pos==d->stage_frames) { int n=next_stage(d,loop);if(n<=0) return total?(int)total:n; }
        unsigned n=d->stage_frames-d->stage_pos;if(n>frames-total) n=frames-total;
        memcpy(stereo+total*2,d->stage+d->stage_pos*2,n*4);d->stage_pos+=n;total+=n;
    }
    return (int)total;
}
