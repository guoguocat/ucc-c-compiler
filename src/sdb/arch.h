#ifndef ARCH_H
#define ARCH_H

#include <sys/types.h> /* pid_t */

typedef unsigned long reg_t;
typedef unsigned long addr_t;

enum arch_reg
{
	/* start at one to avoid arch_reg_read(..., (void *)0, ...) */
	ARCH_REG_IP = 1,
};

const char **arch_reg_names(void);

reg_t arch_reg_read( pid_t pid, const char *nam);
void  arch_reg_write(pid_t pid, const char *nam, reg_t);

reg_t arch_reg_read_e( pid_t pid, enum arch_reg);
void  arch_reg_write_e(pid_t pid, enum arch_reg, reg_t);

int arch_read( pid_t, addr_t,       void *, size_t);
int arch_write(pid_t, addr_t, const void *, size_t);

unsigned long arch_inst_trap(void);

#endif
