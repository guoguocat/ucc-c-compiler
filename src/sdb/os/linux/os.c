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

int arch_read(pid_t pid, addr_t addr, void *vp, size_t l)
{
	unsigned long *p = vp;

	while(l){
		errno = 0;
		unsigned long buf = ptrace(PTRACE_PEEKTEXT, pid, addr, 0);
		if(errno)
			return -1;

		switch(l){
#define CASE(ty)            \
			case sizeof(ty):      \
				*(ty *)p = (ty)buf; \
				goto out

			CASE(char);
			CASE(short);
			CASE(int);

			case sizeof(long): /* assume 64-bit */
				*p++ = buf;
				l -= sizeof(long);
		}
	}

out:
	return 0;
}

int arch_write(pid_t pid, addr_t addr, const void *p, size_t l)
{
	errno = ENOSYS;
	return -1;
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
