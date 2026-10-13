/* Actual stream context/cancellation/options; FFmpeg and direct memory mocked. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <libavutil/error.h>
#include "../../app/engine/media/src/evo_stream_io.c"
struct AVDictionary { int unused; };
static char reconnect_eof[8];
static int live_memory=0, fail_open=0;
int av_dict_set(AVDictionary **d,const char *key,const char *value,int flags) {
    (void)d;(void)flags;
    if(strcmp(key,"reconnect_at_eof")==0)snprintf(reconnect_eof,sizeof reconnect_eof,"%s",value);
    return 0;
}
void av_dict_free(AVDictionary **d){*d=NULL;}
void *evo_direct_mem_calloc(size_t count,size_t size){void *p=calloc(count,size);if(p)++live_memory;return p;}
void evo_direct_mem_free(void *p){if(p){--live_memory;free(p);}}
AVFormatContext *avformat_alloc_context(void){return calloc(1,sizeof(AVFormatContext));}
int avformat_open_input(AVFormatContext **f,const char *url,const AVInputFormat *fmt,AVDictionary **d) {
    (void)url;(void)fmt;(void)d;
    assert((*f)->interrupt_callback.callback);
    if(fail_open||(*f)->interrupt_callback.callback((*f)->interrupt_callback.opaque)){
        free(*f);*f=NULL;return AVERROR_EXIT;
    }
    return 0;
}
static int probe_count, cancel_during_probe, timed_probe;
static int *probe_stop;
int avformat_find_stream_info(AVFormatContext *fmt, AVDictionary **opts) {
    (void)opts; ++probe_count;
    if (cancel_during_probe) {
        *probe_stop=1;
        assert(fmt->interrupt_callback.callback(fmt->interrupt_callback.opaque));
    }
    if (timed_probe) {
        usleep(5000);
        assert(fmt->interrupt_callback.callback(fmt->interrupt_callback.opaque));
    }
    return 1; /* FFmpeg may keep headers and swallow the interrupted read. */
}
static int cancelled(void *p){return *(int*)p;}
static void *other_thread(void *p){(void)p;assert(!s_open_interrupt && !s_open_opaque);return NULL;}
int main(void){
    AVDictionary *opts=NULL;
    const char *vod[]={"http://host/movie/u/p/1.ts","http://host/SERIES/u/p/1.MKV","http://host/test.MP4?token=x","http://host/Videos/stream?Static=true","http://host/music.flac","http://host/live/list.m3u8"};
    for(size_t i=0;i<sizeof vod/sizeof vod[0];++i){evo_stream_io_apply_network_options(&opts,vod[i]);assert(strcmp(reconnect_eof,"0")==0);}
    evo_stream_io_apply_network_options(&opts,"http://movie.mp4.example/live/u/p/1.ts");assert(strcmp(reconnect_eof,"1")==0);
    int stop=0;probe_stop=&stop;evo_stream_io_set_open_interrupt(cancelled,&stop);
    pthread_t t;assert(pthread_create(&t,NULL,other_thread,NULL)==0);assert(pthread_join(t,NULL)==0);
    AVFormatContext *fmt=NULL;evo_stream_io_ctx_t *ctx=NULL;
    assert(evo_stream_io_open("http://host/movie/u/p/1.mp4",&fmt,NULL,&ctx)==0);
    assert(ctx&&live_memory==1&&ctx->external_opaque==&stop);
    assert(evo_stream_io_probe(ctx,fmt,1.)==1&&probe_count==1);
    evo_stream_io_set_open_interrupt(NULL,NULL); // context retains the operation hook
    assert(!fmt->interrupt_callback.callback(fmt->interrupt_callback.opaque));
    stop=1;assert(fmt->interrupt_callback.callback(fmt->interrupt_callback.opaque));
    stop=0;assert(sio_interrupt_cb(ctx)); // cancellation remains latched
    free(fmt);evo_stream_io_close(ctx);assert(live_memory==0);
    evo_stream_io_set_open_interrupt(cancelled,&stop);
    assert(evo_stream_io_open("http://host/movie/u/p/1.mp4",&fmt,NULL,&ctx)==0);
    cancel_during_probe=1;
    assert(evo_stream_io_probe(ctx,fmt,1.)==AVERROR_EXIT);
    int counted=probe_count;
    stop=0;cancel_during_probe=0;
    assert(evo_stream_io_probe(ctx,fmt,1.)==AVERROR_EXIT&&probe_count==counted);
    free(fmt);evo_stream_io_close(ctx);assert(live_memory==0);
    evo_stream_io_set_open_interrupt(NULL,NULL);
    assert(evo_stream_io_open("http://host/movie/u/p/1.mp4",&fmt,NULL,&ctx)==0);
    timed_probe=1;
    assert(evo_stream_io_probe(ctx,fmt,.001)==1); /* probe deadline != a user cancellation */
    assert(evo_stream_io_deadline_expired(ctx)&&!evo_stream_io_is_aborted(ctx));
    timed_probe=0;
    evo_stream_io_set_deadline(ctx,.001);usleep(5000);assert(sio_interrupt_cb(ctx));
    assert(evo_stream_io_deadline_expired(ctx));evo_stream_io_set_deadline(ctx,0);
    assert(!sio_interrupt_cb(ctx));evo_stream_io_abort(ctx);assert(sio_interrupt_cb(ctx));
    free(fmt);evo_stream_io_close(ctx);assert(live_memory==0);
    stop=1;evo_stream_io_set_open_interrupt(cancelled,&stop);
    assert(evo_stream_io_open("http://host/movie/u/p/1.mp4",&fmt,NULL,NULL)<0&&live_memory==0);
    evo_stream_io_set_open_interrupt(NULL,NULL);
    puts("PASS: real stream open installs cancellation before FFmpeg, isolates threads, retains context hook, rejects cancelled probes despite success headers, honors deadlines/abort and disables VOD EOF reconnects");
}
