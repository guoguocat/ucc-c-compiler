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

int arch_read(pid_t pid, addr_t addr, void *p, size_t l)
{
	errno = ENOSYS;
	return -1;
}

int arch_write(pid_t pid, addr_t addr, const void *p, size_t l)
{
	errno = ENOSYS;
	return -1;
}

int arch_reg_read(pid_t pid, int i, reg_t *p)
{
	assert(i >= 0);

	*p = ptrace(PTRACE_PEEKUSER, pid, i * sizeof(reg_t), 0);

	return 0;
}

int arch_reg_write(pid_t pid, int i, const reg_t v)
{
	errno = ENOSYS;
	return -1;
}
