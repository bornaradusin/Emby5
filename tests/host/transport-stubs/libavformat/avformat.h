/* Narrow host-test declarations, never a native FFmpeg header replacement. */
#pragma once
#include "avio.h"
#include <libavutil/dict.h>
typedef struct AVFormatContext { AVIOInterruptCB interrupt_callback; } AVFormatContext;
typedef struct AVInputFormat AVInputFormat;
AVFormatContext *avformat_alloc_context(void);
int avformat_open_input(AVFormatContext **,const char *,const AVInputFormat *,AVDictionary **);
int avformat_find_stream_info(AVFormatContext *, AVDictionary **);
