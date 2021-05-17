#include "log.h"

void _log_fprintf(FILE *__restrict stream, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(stream, fmt, args);
	va_end(args);
	// add a new line
	fprintf(stream, "\n");
}

void _log_error(
		const char *func, const char *file, int line, const char *fmt, ...) {
	fprintf(stderr, "ERROR: ");
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n    at %s (%s:%i)\n", func, file, line);
}