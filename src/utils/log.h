#ifndef __LOG_H__
#define __LOG_H__

#include <stdarg.h>
#include <stdio.h>

/**
 * Logs a message with string formatting support.
 */
#define LOG_INFO(...) _log_fprintf(stdout, __VA_ARGS__)
/**
 * Logs a message with string formatting support.
 */
#define LOG_DEBUG(...) _log_fprintf(stdout, __VA_ARGS__)
/**
 * Logs a message with string formatting support.
 */
#define LOG_WARN(...) _log_fprintf(stdout, __VA_ARGS__)
/**
 * Logs error message with location and string formatting support.
 */
#define LOG_ERR(...) _log_error(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)

void _log_fprintf(FILE *__restrict stream, const char *fmt, ...);
void _log_error(
		const char *func, const char *file, int line, const char *fmt, ...);

#endif // __LOG_H__