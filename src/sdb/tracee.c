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

void tracee_traceme()
{
	if(ptrace(PTRACE_TRACEME, 0, 0, 0) < 0)
		die("ptrace(TRACE_ME):");
}

pid_t tracee_create(tracee *t)
{
	memset(t, 0, sizeof *t);

	switch(t->pid = fork()){
		case -1:
			die("fork():");

		case 0:
			break;

		default:
			t->running = 1;
	}

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
		t->running = 0;

	}else{
		warn("unknown waitpid status 0x%x", wstatus);
	}
}

static void tracee_ptrace(int req, pid_t pid, void *addr, void *data)
{
	if(ptrace(req, pid, addr, data) < 0)
		die("ptrace():");
}

void tracee_kill(tracee *t, int sig)
{
	if(kill(t->pid, sig) == -1)
		die("kill():");
}

void tracee_step(tracee *t)
{
	tracee_ptrace(PTRACE_SINGLESTEP, t->pid, 0, 0);
}

void tracee_continue(tracee *t)
{
	tracee_ptrace(PTRACE_CONT, t->pid, 0, 0);
}
