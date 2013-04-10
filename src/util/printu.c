#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>

#include "util.h"
#include "printu.h"

#include "alloc.h"

int vfprintu(FILE *f, const char *fmt, va_list l)
{
	char *newfmt = ustrdup(fmt);
	va_list l_cpy;
	char *p;

	va_copy(l_cpy, l);

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
				const ptrdiff_t di = p - newfmt;
				char *new;

				p[-1] = '\0';

				/* FIXME: can't pass through this va_arg to vfprintf below... */

				new = ustrprintf("%s%s%s",
						newfmt, found, p + 1);
				free(newfmt);
				newfmt = new;
				p = newfmt + di;
			}else{
				/* need to step over it */
				/* this is fine as long as we only pass scalars */
				(void)va_arg(l, long);
			}
		}
	}

	{
		int r = vfprintf(f, newfmt, l_cpy);
		va_end(l_cpy);
		free(newfmt);
		return r;
	}
}

int snprintu(char *buf, size_t len, const char *fmt, ...)
{
	*buf = '?',
	buf[1] = '?',
	buf[2] = 0;
	return 2;
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
