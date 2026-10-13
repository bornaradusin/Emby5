/* Host test declarations only. NEVER included by the PS5 build. */
#pragma once
#include <stdint.h>
typedef struct AVDictionary AVDictionary;
typedef struct AVIOContext AVIOContext;
typedef struct AVIOInterruptCB { int (*callback)(void *); void *opaque; } AVIOInterruptCB;
#define AVIO_FLAG_READ 1
int avio_open2(AVIOContext **,const char *,int,const AVIOInterruptCB *,AVDictionary **);
int avio_closep(AVIOContext **);
int avio_read_partial(AVIOContext *,unsigned char *,int);

int64_t avio_size(AVIOContext *);
