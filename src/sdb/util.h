#ifndef SDB_UTIL_H
#define SDB_UTIL_H

/*#include <stdnoreturn.h>*/
#define noreturn __attribute__((noreturn))

void warn(const char *fmt, ...);
noreturn void die(const char *fmt, ...);

int mkdir_p(char *);

#endif
