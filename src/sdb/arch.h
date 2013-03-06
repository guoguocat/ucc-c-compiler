#ifndef ARCH_H
#define ARCH_H

#include <sys/types.h>

#ifndef __x86_64__
#  error todo: 32-bit
#endif

struct arch_regs
{
#define REG(nam) unsigned long nam;
#include "arch_regs.h"
#undef REG
};

void arch_read_regs( pid_t pid, struct arch_regs *);
void arch_write_regs(pid_t pid, struct arch_regs *);

#endif
