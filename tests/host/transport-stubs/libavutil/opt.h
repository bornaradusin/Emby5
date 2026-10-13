#pragma once
#include <stdint.h>
#define AV_OPT_SEARCH_CHILDREN 1
int av_opt_get(void *,const char *,int,uint8_t **);
