#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <signal.h>

#include "os/ptrace.h"
#include "arch.h"
#include "tracee.h"
#include "util.h"

#include "../util/dynarray.h"

void tracee_traceme()
{
	if(os_ptrace(SDB_TRACEME, 0, 0, 0) < 0)
		die("ptrace(TRACE_ME):");
}

pid_t tracee_create(tracee *t)
{
	memset(t, 0, sizeof *t);

	if((t->pid = fork()) == -1)
		die("fork():");

	return t->pid;
}

void tracee_wait(tracee *t)
{
	int wstatus;

	if(waitpid(t->pid, &wstatus, 0) == -1)
		goto buh;

	if(WIFSTOPPED(wstatus)){
		t->event = TRACEE_TRAPPED;

	}else if(WIFSIGNALED(wstatus)){
		t->event = TRACEE_SIGNALED;
		t->sig = WSTOPSIG(wstatus);

	}else if(WIFEXITED(wstatus)){
		t->event = TRACEE_KILLED;
		t->exit_code = WEXITSTATUS(wstatus);

	}else{
buh:
		warn("unknown waitpid status 0x%x", wstatus);
	}
}

static void tracee_ptrace(int req, pid_t pid, void *addr, void *data)
{
	if(os_ptrace(req, pid, addr, data) < 0)
		warn("ptrace():");
}

void tracee_kill(tracee *t, int sig)
{
	if(kill(t->pid, sig) == -1)
		warn("kill():");
}

int tracee_alive(tracee *t)
{
	return kill(t->pid, 0) != -1;
}

#define SIG_ARG_NONE 0
#ifdef __APPLE__
#  define ADDR_ARG_NONE (void *)1
#else
#  define ADDR_ARG_NONE 0
#endif

void tracee_step(tracee *t)
{
	tracee_ptrace(SDB_SINGLESTEP, t->pid,
			ADDR_ARG_NONE, SIG_ARG_NONE);
}

void tracee_continue(tracee *t)
{
	tracee_ptrace(SDB_CONT, t->pid,
			ADDR_ARG_NONE, SIG_ARG_NONE);
}

reg_t tracee_read_reg(tracee *t, const char *nam)
{
	return arch_reg_read(t->pid, nam);
}

int tracee_break(tracee *t, addr_t a)
{
	bkpt *b = bkpt_new(t->pid, a);
	if(!b)
		return -1;

	dynarray_add((void ***)&t->breakpoints, b);
	return 0;
}
