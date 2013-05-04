#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <sys/ptrace.h>

#include "../ptrace.h"
#include "../../arch.h"
#include "../../util.h"

int arch_reg_offset(const char *s)
{
#define REG(x) if(!strcasecmp(s, #x)) return x;
#include "regs.def"
#undef REG
	return -1;
}

struct arch_proc *arch_attach(pid_t pid)
{
	struct arch_proc *ap = calloc(1, sizeof *ap);
	ap->pid = pid;
	return ap;
}

void arch_detach(struct arch_proc **pap)
{
	free(*pap);
	*pap = NULL;
}

int arch_mem_read(pid_t pid, addr_t addr, unsigned long *p)
{
	errno = 0;
	*p = ptrace(PTRACE_PEEKTEXT, pid, addr, 0);
	if(errno)
		return -1;

	return 0;
}

int arch_mem_write(pid_t pid, addr_t addr, unsigned long v)
{
	return ptrace(PTRACE_POKETEXT, pid, addr, v) != 0;
}

int arch_reg_read(pid_t pid, int i, reg_t *p)
{
	assert(i >= 0);

	errno = 0;
	*p = ptrace(PTRACE_PEEKUSER, pid, i * sizeof(reg_t), 0);

	return errno ? -1 : 0;
}

int arch_reg_write(pid_t pid, int i, const reg_t v)
{
	assert(i >= 0);

	return ptrace(PTRACE_POKEUSER, pid, i * sizeof(reg_t), v) < 0 ? -1 : 0;
}
