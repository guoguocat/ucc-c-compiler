#include <string.h>
#include <stddef.h> /* offsetof */
#include <assert.h>

#include <sys/reg.h>
#include <sys/user.h>

#include "../ptrace.h"
#include "../../arch.h"
#include "../../util.h"

static int reg_offset(const char *nam)
{
#define REG(r) if(!strcmp(nam, #r)) return offsetof(struct user_regs_struct, r);
#include "regs.def"
#undef REG
	return -1;
}

static unsigned long *reg_addr(struct user_regs_struct *regs, const char *nam)
{
	int off = reg_offset(nam);
	assert(off != -1);
	return (unsigned long *)((char *)regs + off);
}

const char **arch_reg_names(void)
{
	static const char *regs[] = {
#define REG(r) #r,
#include "regs.def"
#undef REG
		NULL
	};

	return regs;
}

unsigned long arch_reg_read(pid_t pid, const char *nam)
{
	if(reg_offset(nam) == -1){
		/* TODO: pass back error */
		warn("register %s not found", nam);
		return -1UL;
	}

	struct user usr;
	if(ptrace(PTRACE_GETREGS, pid, 0, &usr) < 0){
		warn("ptrace(PTRACE_GETREGS):");
		return -1UL;
	}

	return *reg_addr(&usr.regs, nam);
}

void arch_reg_write(pid_t pid, const char *nam, unsigned long val)
{
	// TODO
}
