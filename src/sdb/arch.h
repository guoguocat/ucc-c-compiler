#ifndef ARCH_H
#define ARCH_H

#include <sys/types.h> /* pid_t */

typedef unsigned long reg_t;
typedef unsigned long addr_t;

const char **arch_reg_names(void);

reg_t arch_reg_read( pid_t pid, const char *nam);
void  arch_reg_write(pid_t pid, const char *nam, reg_t);

int arch_read( pid_t, addr_t,       char *, size_t);
int arch_write(pid_t, addr_t, const char *, size_t);

unsigned long arch_inst_trap(void);

#endif
