#ifndef UPRINTF_H
#define UPRINTF_H

/*
 * like printf but:
 * %D - decl *
 * %R - type_ref *
 * %T - type *
 */

int uvfprintf(FILE *, const char *, va_list) ucc_printflike(1, 0);
int ufprintf(FILE *, const char *, ...) ucc_printflike(1, 2);

int uprintf(const char *, ...) ucc_printflike(1, 2);

/* this may require considerable factoring of the {die,warn}_* functions
 * perhaps we go with cc1_die
 */

#endif
