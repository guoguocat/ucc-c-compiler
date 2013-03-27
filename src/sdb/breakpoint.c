#include <stdlib.h>
#include <stdarg.h>

#include "arch.h"
#include "breakpoint.h"

#include "../util/alloc.h"

typedef unsigned long ulong;

struct bkpt
{
	addr_t addr;
	ulong orig_code;
	pid_t pid;
};

int bkpt_place(bkpt *b, pid_t pid, addr_t a)
{
	b->addr = a;
	b->pid = pid;
	return bkpt_enable(b);
}

bkpt *bkpt_new(addr_t a, pid_t pid)
{
	bkpt *b = umalloc(sizeof *b);
	if(bkpt_place(b, pid, a))
		free(b), b = NULL;
	return b;
}

int bkpt_enable(bkpt *b)
{
	ulong code;
	if(arch_read(b->pid, b->addr, &code, sizeof code))
		return -1;

	b->orig_code = code;

	char trap = arch_inst_trap();

	/* little endian - fine */
	if(arch_write(b->pid, b->addr, &trap, sizeof trap))
		return -1;

	return 0;
}

int bkpt_disable(bkpt *b)
{
	return arch_write(b->pid, b->addr, &b->orig_code, sizeof b->orig_code);
}
