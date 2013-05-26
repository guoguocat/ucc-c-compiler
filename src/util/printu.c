#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "util.h"
#include "printu.h"
#include "dynarray.h"

#include "alloc.h"

static char *sprintu(const char *fmt, va_list l)
{
	char **join_us = NULL;
	char *newfmt = ustrdup(fmt);
	char *p, *anchor = newfmt;
	char *ret;

	for(p = newfmt; *p; p++){
		if(*p == '%'){
			const char *found = NULL;
			int i;

			p++;
			/* builtin */
			if(*p == 'W'){
				found = where_str(va_arg(l, where *));
			}else{
				/* custom */
				for(i = 0; printu_extras[i].fn; i++){
					if(printu_extras[i].fmt == *p){
						found = printu_extras[i].fn(va_arg(l, void *));
						break;
					}
				}
			}

			if(found){
				char *up_to, *both;

				fprintf(stderr, "PRINTU: \"%s\"\n", found);

				*p = '\0';
				/* eat up however many elements from l */
				up_to = ustrvprintf(anchor, l);

				both = ustrprintf("%s%s", up_to, found);

				free(up_to);
				dynarray_add(&join_us, both);

				fprintf(stderr, "BOTH: \"%s\"\n", both);
				anchor = p + 1;
				fprintf(stderr, "ANCHOR: \"%s\"\n", anchor);
			}
			/* else ignore for now... */
		}
	}

	if(anchor != p)
		dynarray_add(&join_us, ustrvprintf(anchor, l));

	ret = str_join(join_us, "");

	dynarray_free(char **, &join_us, free);
	free(newfmt);

	return ret;
}

int vfprintu(FILE *f, const char *fmt, va_list l)
{
	char *s = sprintu(fmt, l);
	size_t len = strlen(s);

	fputs(s, f);

	free(s);
	return len;
}

int snprintu(char *buf, size_t len, const char *fmt, ...)
{
	va_list l;
	char *s;
	size_t r;

	va_start(l, fmt);
	s = sprintu(fmt, l);
	va_end(l);

	r = snprintf(buf, len, "%s", s);

	free(s);

	return r;
}

int fprintu(FILE *f, const char *fmt, ...)
{
	va_list l;
	int r;
	va_start(l, fmt);
	r = vfprintu(f, fmt, l);
	va_end(l);
	return r;
}

int printu(const char *fmt, ...)
{
	va_list l;
	int r;
	va_start(l, fmt);
	r = vfprintu(stdout, fmt, l);
	va_end(l);
	return r;
}
