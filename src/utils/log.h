#ifndef __LOG_H__
#define __LOG_H__

#include <stdarg.h>
#include <stdio.h>

#define LOG(...) log_fprintf(stdout, __VA_ARGS__)
#define LOG_ERR(...) log_fprintf(stderr, __VA_ARGS__)

void log_fprintf(FILE *__restrict stream, const char *fmt, ...);

#endif // __LOG_H__