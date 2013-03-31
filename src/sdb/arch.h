#ifndef ARCH_H
#define ARCH_H

#include <sys/types.h> /* pid_t */

typedef unsigned long reg_t;
typedef unsigned long addr_t;
typedef unsigned long word_t;

enum pseudo_reg
{
	ARCH_REG_IP,
	ARCH_REG_SP,
};

const char **arch_reg_names(void);

int arch_reg_offset(const char *);

int arch_pseudo_reg(enum pseudo_reg);

int arch_reg_read( pid_t pid, int off, reg_t *);
int arch_reg_write(pid_t pid, int off, const reg_t);

int arch_mem_read( pid_t, addr_t, word_t *);
int arch_mem_write(pid_t, addr_t, word_t);

unsigned long arch_trap_inst(void);
unsigned long arch_trap_mask(void);
unsigned long arch_trap_size(void);

#endif
