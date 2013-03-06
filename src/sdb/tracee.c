#define _POSIX_SOURCE
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <signal.h>

#include <sys/ptrace.h>

#if 0
#include <signal.h>
#include <syscall.h>
#include <sys/reg.h>
#include <sys/user.h>
#endif

#include "tracee.h"
#include "util.h"

#ifdef __APPLE__
#  define PTRACE_TRACEME    PT_TRACE_ME
#  define PTRACE_SINGLESTEP PT_STEP
#  define PTRACE_CONT       PT_CONTINUE
#  define DATA_CAST(x) (intptr_t)x
#else
#  define DATA_CAST(x) x
#endif

void tracee_traceme()
{
	if(ptrace(PTRACE_TRACEME, 0, 0, 0) < 0)
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
		die("waitpid():");

	if(WIFSTOPPED(wstatus)){
		t->event = TRACEE_TRAPPED;

	}else if(WIFSIGNALED(wstatus)){
		t->event = TRACEE_SIGNALED;
		t->sig = WSTOPSIG(wstatus);

	}else if(WIFEXITED(wstatus)){
		t->event = TRACEE_KILLED;
		t->exit_code = WEXITSTATUS(wstatus);

	}else{
		warn("unknown waitpid status 0x%x", wstatus);
	}
}

static void tracee_ptrace(int req, pid_t pid, void *addr, void *data)
{
	if(ptrace(req, pid, addr, DATA_CAST(data)) < 0)
		die("ptrace():");
}

void tracee_kill(tracee *t, int sig)
{
	if(kill(t->pid, sig) == -1)
		die("kill():");
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
	tracee_ptrace(PTRACE_SINGLESTEP, t->pid,
			ADDR_ARG_NONE, SIG_ARG_NONE);
}

void tracee_continue(tracee *t)
{
	tracee_ptrace(PTRACE_CONT, t->pid,
			ADDR_ARG_NONE, SIG_ARG_NONE);
}
