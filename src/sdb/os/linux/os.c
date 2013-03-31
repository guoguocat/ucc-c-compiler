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

int arch_pseudo_reg(enum pseudo_reg r)
{
	switch(r){
		case ARCH_REG_IP: return RIP;
		case ARCH_REG_SP: return RSP;
	}
	return -1;
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

int arch_usr_read(pid_t pid, unsigned off, reg_t *p)
{
	errno = 0;
	*p = ptrace(PTRACE_PEEKUSER, pid, off, 0);

	return errno ? -1 : 0;
}

int arch_usr_write(pid_t pid, unsigned off, const reg_t v)
{
	return ptrace(PTRACE_POKEUSER, pid, off, v) < 0 ? -1 : 0;
}

int arch_reg_read(pid_t pid, unsigned i, reg_t *p)
{
	return arch_usr_read(pid, i * sizeof(reg_t), p);
}

int arch_reg_write(pid_t pid, unsigned i, const reg_t v)
{
	return arch_usr_write(pid, i * sizeof(reg_t), v);
}
