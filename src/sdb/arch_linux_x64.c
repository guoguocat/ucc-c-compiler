#include <string.h>

#include <sys/reg.h>
#include <sys/user.h>

#include "arch.h"
#include "ptrace.h"
#include "util.h"

void arch_read_regs(pid_t pid, struct arch_regs *regs)
{
	struct user usr;

	if(sdb_ptrace(PTRACE_GETREGS, pid, 0, &usr) < 0){
		warn("ptrace(PTRACE_GETREGS):");
		memset(regs, 0, sizeof *regs);
		return;
	}

#define REG(nam) regs->nam = usr.regs.nam;
#include "arch_regs.h"
#undef REG
}

void arch_write_regs(pid_t pid, struct arch_regs *regs)
{
	/* TODO */
	(void)pid;
	(void)regs;
}
