#include <strings.h>
#include <errno.h>

#include <sys/user.h>
#include <sys/ptrace.h>

#include "../../arch.h"

int arch_reg_offset(const char *s)
{
//#define REG(x) if(!strcasecmp(s, #x)) return x;
//#include "regs.def"
//#undef REG
	return 0;
	return -1;
}

int arch_pseudo_reg(enum pseudo_reg r)
{
	switch(r){
		case ARCH_REG_IP: return 0; //RIP;
		case ARCH_REG_SP: return 1; //RSP;
	}
	return -1;
}

int arch_mem_read(pid_t pid, addr_t addr, word_t *p)
{
	errno = ENOSYS;
	return -1;
}

int arch_mem_write(pid_t pid, addr_t addr, word_t l)
{
	errno = ENOSYS;
	return -1;
}

int arch_reg_read(pid_t pid, int off, reg_t *p)
{
	errno = ENOSYS;
	return -1;
}

int arch_reg_write(pid_t pid, int off, const reg_t v)
{
	// TODO
	errno = ENOSYS;
	return -1;
}
