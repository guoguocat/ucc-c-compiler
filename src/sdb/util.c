#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "util.h"

static void vwarn(const char *fmt, va_list l)
{
	vfprintf(stderr, fmt, l);

	size_t len = strlen(fmt);
	if(len > 0 && fmt[len-1] == ':')
		fprintf(stderr, " %s", strerror(errno));

	fputc('\n', stderr);
}

void warn(const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
	vwarn(fmt, l);
	va_end(l);
}

void die(const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
	vwarn(fmt, l);
	va_end(l);

	exit(1);
}
