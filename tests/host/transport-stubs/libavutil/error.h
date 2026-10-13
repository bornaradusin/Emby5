/* Values match FFmpeg error tags; transport behavior is mocked separately. */
#pragma once
#include <stddef.h>
#define AVERROR(e) (-(e))
#define ERR_TAG(a,b,c,d) (-(int)((unsigned)(a)|((unsigned)(b)<<8)|((unsigned)(c)<<16)|((unsigned)(d)<<24)))
#define AVERROR_HTTP_BAD_REQUEST ERR_TAG(0xF8,'4','0','0')
#define AVERROR_HTTP_UNAUTHORIZED ERR_TAG(0xF8,'4','0','1')
#define AVERROR_HTTP_FORBIDDEN ERR_TAG(0xF8,'4','0','3')
#define AVERROR_HTTP_NOT_FOUND ERR_TAG(0xF8,'4','0','4')
#define AVERROR_HTTP_OTHER_4XX ERR_TAG(0xF8,'4','X','X')
#define AVERROR_HTTP_SERVER_ERROR ERR_TAG(0xF8,'5','X','X')
#define AVERROR_EXIT ERR_TAG('E','X','I','T')
#define AVERROR_EOF ERR_TAG('E','O','F',' ')
int av_strerror(int,char *,size_t);
