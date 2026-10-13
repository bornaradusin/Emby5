#pragma once
#include <stdint.h>
typedef struct AVDictionary AVDictionary;
int av_dict_set(AVDictionary **,const char *,const char *,int);
int av_dict_set_int(AVDictionary **,const char *,int64_t,int);
void av_dict_free(AVDictionary **);
