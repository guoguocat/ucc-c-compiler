#include <stdlib.h>
#include <stdarg.h>

#include "arch.h"
#include "breakpoint.h"

#include "../util/alloc.h"

struct bkpt
{
	addr_t addr;
	word_t orig_code;
	pid_t pid;
};

int bkpt_place(bkpt *b, pid_t pid, addr_t a)
{
	b->addr = a;
	b->pid = pid;
	return bkpt_enable(b);
}

bkpt *bkpt_new(pid_t pid, addr_t a)
{
	bkpt *b = umalloc(sizeof *b);
	if(bkpt_place(b, pid, a))
		free(b), b = NULL;
	return b;
}

int bkpt_enable(bkpt *b)
{
	word_t code;
	if(arch_mem_read(b->pid, b->addr, &code))
		return -1;

	b->orig_code = code;

	code = (code & arch_trap_mask()) | arch_trap_inst();

	if(arch_mem_write(b->pid, b->addr, code))
		return -1;

	return 0;
}

int bkpt_disable(bkpt *b)
{
	return arch_mem_write(b->pid, b->addr, b->orig_code);
}

addr_t bkpt_addr(bkpt *b)
{
	return b->addr;
}
