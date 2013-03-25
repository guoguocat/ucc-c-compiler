#ifndef ARCH_H
#define ARCH_H

#include <sys/types.h> /* pid_t */

const char **arch_reg_names(void);

unsigned long arch_reg_read( pid_t pid, const char *nam);
void          arch_reg_write(pid_t pid, const char *nam, unsigned long);

#endif
