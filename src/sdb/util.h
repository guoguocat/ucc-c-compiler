#ifndef SDB_UTIL_H
#define SDB_UTIL_H

/*#include <stdnoreturn.h>*/
#define noreturn __attribute__((noreturn))
#define attr_printf(a, b) __attribute__((format(printf, a, b)))

attr_printf(1, 2) void warn(const char *fmt, ...);
attr_printf(1, 2) noreturn void die(const char *fmt, ...);

int mkdir_p(char *);

#endif
