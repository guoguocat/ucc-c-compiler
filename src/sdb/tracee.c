#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

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

	pid_t c = fork();
	switch(c){
		case -1:
			die("fork():");

		case 0:
			break;

		default:
			t->running = 1;
	}

	return c;
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

#if 0
void tracee_step(tracee *t)
{
	if(ptrace(PTRACE_SINGLESTEP, child_pid, 0, 0) < 0)
		die("ptrace():");

	n_insts++;

	/* Wait for child to stop on its next instruction */
	wait(&wait_status);
}
#endif
