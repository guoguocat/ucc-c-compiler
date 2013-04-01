#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

/* TODO: abstract */
#include <sys/user.h>
#include <stddef.h>
#define DR(x) offsetof(struct user, u_debugreg[x])
#include <errno.h>

#include "util.h"
#include "arch.h"
#include "watchpoint.h"

#include "../util/alloc.h"

struct watchpt
{
	pid_t pid;
	addr_t mem;
	size_t memsz;
};

int watchpt_place(watchpt *w, pid_t pid, addr_t addr, size_t sz)
{
	w->pid = pid;
	w->mem = addr;
	w->memsz = sz;

	return watchpt_enable(w, 0);
}

watchpt *watchpt_new(pid_t pid, addr_t a, size_t s)
{
	watchpt *w = umalloc(sizeof *w);
	if(watchpt_place(w, pid, a, s))
		free(w), w = NULL;
	return w;
}

int watchpt_enable(watchpt *w, int reg_idx)
{
	assert(0 <= reg_idx && reg_idx < 4);

	reg_t dr7;
	if(arch_usr_read(w->pid, DR(7), &dr7)){
		warn("read dr7:");
		return -1;
	}

	/* bits 0, 2, 4 and 6 enable pid-local watches */
	/* Bits 16-17 (DR0), 20-21 (DR1), 24-25 (DR2), 28-29 (DR3) */

	if(arch_usr_write(w->pid, DR(reg_idx), w->mem)){
		warn("write dr%d:", reg_idx);
		return -1;
	}

	dr7 |= 1 << (0 /* 1 for global */ + 2 * reg_idx);

	dr7 |= 1 << (8 + 2 * reg_idx); /* ??? */

	/* TODO: type */
#define MEM_EXEC  0x0
#define MEM_WRITE 0x1
#define MEM_RW    0x3

	/* now enable the dr%d watch via DR7 */
	dr7 |= MEM_RW << (16 + 4 * reg_idx);
	/* DR0 = 16-17
	 * DR1 = 20-21
	 * DR2 = 24-25
	 * DR3 = 28-29
	 */

	/* TODO: size */
#define MEM_SZ_1 0x0
#define MEM_SZ_2 0x1
#define MEM_SZ_8 0x2
#define MEM_SZ_4 0x3

	dr7 |= MEM_SZ_4 << (18 + 4 * reg_idx);
	/* 18-19 (DR0)
	 * 22-23 (DR1)
	 * 26-27 (DR2)
	 * 30-31 (DR3)
	 */

	if(arch_usr_write(w->pid, DR(7), dr7)){
		warn("write dr7:");
		return -1;
	}

	return 0;
}

int watchpt_disable(watchpt *w, int i)
{
	errno = ENOSYS;
	return -1;
}
