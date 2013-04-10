#ifndef PRINTU_H
#define PRINTU_H

int vfprintu(FILE *, const char *, va_list) ucc_printflike(2, 0);
int fprintu(FILE *, const char *, ...) ucc_printflike(2, 3);

int snprintu(char *, size_t, const char *fmt, ...) ucc_printflike(3, 4);

int printu(const char *, ...) ucc_printflike(1, 2);

typedef const char *printu_f(void *);
extern const struct printu
{
	printu_f *fn;
	char fmt;
} printu_extras[]; /* { NULL, 0 } terminated */

#endif
