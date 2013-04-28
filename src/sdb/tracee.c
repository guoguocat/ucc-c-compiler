#include <string.h>
#include <unistd.h>
#include <errno.h>

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

static bkpt *tracee_find_breakpoint(tracee *t, addr_t loc)
{
	loc -= arch_trap_size(); /* step back over it */

	for(bkpt **i = t->bkpts;
			i && *i;
			i++)
	{
		bkpt *b = *i;
		if(bkpt_addr(b) == loc)
			return b;
	}

	return NULL;
}

int tracee_get_reg(tracee *t, enum pseudo_reg r, reg_t *p)
{
	return arch_reg_read(t->pid,
			arch_pseudo_reg(r), p);
}

int tracee_set_reg(tracee *t, enum pseudo_reg r, const reg_t v)
{
	return arch_reg_write(t->pid,
			arch_pseudo_reg(r), v);
}

static void tracee_eval_sig(tracee *t, reg_t ip, int sig)
{
	switch(sig){
		case SIGTRAP:
			/* check if it's from our breakpoints */
			if((t->evt.bkpt = tracee_find_breakpoint(t, ip))){
				t->event = TRACEE_BREAK;
				break;
			}
			/* fall */

		default:
			t->event = TRACEE_SIGNALED;
			t->evt.sig = sig;
	}
}

void tracee_wait(tracee *t, reg_t *p_ip)
{
	int wstatus;
retry:
	if(waitpid(t->pid, &wstatus, 0) == -1){
		if(errno == EINTR)
			goto retry;
		warn("waitpid():");
		goto buh;
	}

	if(WIFEXITED(wstatus)
	|| (kill(t->pid, 0) == -1 && errno == ESRCH))
	{
		/* dead */
		if(WIFSIGNALED(wstatus)){
			t->event = TRACEE_KILLED;
			t->evt.sig = WTERMSIG(wstatus);
		}else{
			t->event = TRACEE_EXITED;
			t->evt.exit_code = WEXITSTATUS(wstatus);
		}
		return;
	}

	reg_t ip;
	if(tracee_get_reg(t, ARCH_REG_IP, &ip))
		warn("read IP:");
	if(p_ip)
		*p_ip = ip;

	if(WIFSTOPPED(wstatus)){
		tracee_eval_sig(t, ip, WSTOPSIG(wstatus));

	}else if(WIFSIGNALED(wstatus)){
		tracee_eval_sig(t, ip, WTERMSIG(wstatus));

	}else{
		warn("unknown waitpid status 0x%x", wstatus);
buh:
		t->event = TRACEE_SIGNALED;
		t->evt.sig = 0;
	}
}

static void tracee_ptrace(tracee *t, int req, void *addr, void *data)
{
	if(os_ptrace(req, t->pid, addr, data) < 0)
		warn("ptrace():");
}

void tracee_kill(tracee *t, int sig)
{
	if(kill(t->pid, sig) == -1)
		warn("kill():");
}

int tracee_alive(tracee *t)
{
	switch(t->event){
		case TRACEE_KILLED:
		case TRACEE_EXITED:
			return 0;
		case TRACEE_BREAK:
		case TRACEE_SIGNALED:
			break;
	}
	return 1;
}

#define SIG_ARG_NONE 0
#ifdef __APPLE__
#  define ADDR_ARG_NONE (void *)1
#else
#  define ADDR_ARG_NONE 0
#endif

static int tracee_step_bkpt(tracee *t)
{
	if(t->event == TRACEE_BREAK){
		/* resume from breakpoint:
		 * need to disable it, and
		 * step back over it to re-run
		 */
		bkpt *b = t->evt.bkpt;

		if(bkpt_disable(b))
			warn("disable breakpoint:");

		if(tracee_set_reg(t, ARCH_REG_IP, bkpt_addr(b)))
			warn("set ip:");

		/* step over the breakpoint,
		 * then re-enable */
		tracee_ptrace(t, SDB_SINGLESTEP,
				ADDR_ARG_NONE, SIG_ARG_NONE);

		tracee_wait(t, NULL);
		if(tracee_alive(t)){
			if(bkpt_enable(b))
				warn("enable breakpoint:");
		}

		/* continue */
		return 1;
	}
	return 0;
}

void tracee_step(tracee *t)
{
	tracee_step_bkpt(t);

	tracee_ptrace(t, SDB_SINGLESTEP,
			ADDR_ARG_NONE, SIG_ARG_NONE);
}

void tracee_continue(tracee *t)
{
	tracee_step_bkpt(t);

	tracee_ptrace(t, SDB_CONT,
			ADDR_ARG_NONE, SIG_ARG_NONE);
}

int tracee_break(tracee *t, addr_t a)
{
	bkpt *b = bkpt_new(t->pid, a);
	if(!b)
		return -1;

	dynarray_add((void ***)&t->bkpts, b);
	return 0;
}
