#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <signal.h>
#include <unistd.h>

#include "util.h"

#include "arch.h"
#include "tracee.h"
#include "cmds.h"
#include "prompt.h"

static noreturn void
run_target(char **argv)
{
	tracee_traceme();

	execvp(*argv, argv);

	die("exec(\"%s\"):", *argv);
}

static const char *
sdb_signal_name(int sig)
{
	static const char *sigs[] = {
#define SIG(x) [SIG##x] = "SIG"#x
		SIG(HUP),  SIG(INT),  SIG(QUIT), SIG(ILL),
		SIG(TRAP), SIG(ABRT), SIG(FPE),  SIG(KILL),
		SIG(USR1), SIG(SEGV), SIG(USR2), SIG(PIPE),
		SIG(ALRM), SIG(TERM), SIG(CHLD), SIG(CONT),
		SIG(STOP), SIG(TSTP), SIG(TTIN), SIG(TTOU),
#undef SIG
	};

	if(sig < (signed)(sizeof(sigs)/sizeof(*sigs)) && sigs[sig])
		return sigs[sig];

	static char buf[8];
	snprintf(buf, sizeof buf, "%d", sig);
	return buf;
}

static tracee *current_child;

static void
pass_sig(int sig)
{
	if(tracee_alive(current_child)
	&& kill(current_child->pid, sig))
	{
		warn("kill(%d, %d):", current_child->pid, sig);
	}
}

static void
sdb_signal(int sig, void (*pf)(int))
{
	int r = sigaction(SIGINT,
			&(struct sigaction){
				.sa_handler = pf, },
			NULL);

	if(r)
		warn("signal(%s, ...):", sdb_signal_name(sig));
}

static noreturn void
run_debugger(tracee *child)
{
	current_child = child;
	sdb_signal(SIGINT, pass_sig);

	printf("child pid %d\n", child->pid);

	for(;;){
		reg_t ip;
		tracee_wait(child, &ip);

		switch(child->event){
			case TRACEE_KILLED:
				printf("exited with code %d\n", child->evt.exit_code);
				break;

			case TRACEE_SIGNALED:
			{
				handle_ops *hops = handle_ops(child->evt.sig);

				if(hops->print)
					printf("signaled with %s @ " REG_FMT "\n",
							sdb_signal_name(child->evt.sig), ip);

				if(hops->hereditary)
					tracee_kill(child, child->evt.sig);

				if(!hops->stop){
					tracee_continue(child);
					continue;
				}
				break;
			}

			case TRACEE_BREAK:
				printf("stopped @ breakpoint " REG_FMT "\n",
						bkpt_addr(child->evt.bkpt));
				break;
		}

prompt:
		fputs("(sdb) ", stdout);

		char **cmd;
		if(!(cmd = prompt())){
			if(errno == EINTR){
				puts("interrupt");
				goto prompt;
			}
			putchar('\n');
			c_quit(child, (char *[]){"q", NULL});
		}

		if(!cmd_dispatch(child, cmd))
			goto prompt;
	}
}

static void
setup_dir(char *dir)
{
	return; /* for now, we're command-line */
	if(mkdir_p(dir))
		die("mkdir %s:", dir);

	if(chdir(dir))
		die("chdir %s:", dir);

	printf("success %s\n", dir);
	exit(1);
	// fifo
}

int
main(int argc, char **argv)
{
	char *d = NULL;
	unsigned i = 1;

	if(argc > 2 && !strcmp(argv[i], "-d")){
		d = argv[++i];
		i++;
	}

	char buf[16];
	if(!d)
		snprintf(buf, sizeof buf, "sdb.%d", getpid());
	setup_dir(d ? d : buf);

	tracee child;
	switch(tracee_create(&child)){
		case 0:
			run_target(argv + i);
			break;

		default:
			run_debugger(&child);
	}

	fprintf(stderr, "Usage: %s [-d dir] [command [args...]]\n", *argv);
	return 1;
}
