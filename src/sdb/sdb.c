#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
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

static noreturn void
run_debugger(tracee *child)
{
	printf("child pid %d\n", child->pid);

	for(;;){
		reg_t ip;
		tracee_wait(child, &ip);

		switch(child->event){
			case TRACEE_KILLED:
				printf("exited with code %d\n", child->evt.exit_code);
				break;

			case TRACEE_SIGNALED:
				printf("signaled with signal %d @ " REG_FMT "\n",
						child->evt.sig, ip);
				break;

			case TRACEE_BREAK:
				printf("stopped @ breakpoint " REG_FMT "\n",
						bkpt_addr(child->evt.bkpt));
				break;

			case TRACEE_TRAPPED:
				printf("trapped @ " REG_FMT "\n", ip);
				break;
		}

prompt:
		fputs("(sdb) ", stdout);

		char **cmd;
		if(!(cmd = prompt())){
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
