#include "log.h"

void log_fprintf(FILE *__restrict stream, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(stream, fmt, args);
	va_end(args);
	// add a new line
	fprintf(stream, "\n");
}