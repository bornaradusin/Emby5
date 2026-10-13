/* Declaration-only host syntax fixture, not an FFmpeg implementation. */
#pragma once
#include <stdarg.h>
#define AV_LOG_WARNING 24
void av_log_set_callback(void (*)(void *, int, const char *, va_list));
void av_log_set_level(int);
