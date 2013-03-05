#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "util.h"
#include "tracee.h"

static noreturn void
run_target(char **argv)
{
	tracee_traceme();

	execvp(*argv, argv);

	die("exec(\"%s\"):", *argv);
}

static void
c_kill(tracee *child)
{
	tracee_kill(child, SIGKILL);
}

static noreturn void
c_quit(tracee *child)
{
	if(child->running)
		c_kill(child);
	exit(0);
}

static void
c_break(tracee *child)
{
	/* TODO */
}

static void
c_examine(tracee *child)
{
	/* TODO */
}

static const struct
{
	const char *s;
	void (*f)(tracee *);
	int needs_alive;
} cmds[] = {
	{ "quit", c_quit, 0 },
	{ "break", c_break, 0 },
	{ "x", c_examine, 1 },
	{ "kill", c_kill, 1 },

	{ "cont", tracee_continue, 1 },
	{ "step", tracee_step, 1 },

	{ NULL }
};

static int
dispatch(tracee *child, const char *cmd)
{
	/* TODO: parse cmd */
	int dispatched = 0, found = 0;

	for(int i = 0; cmds[i].s; i++)
		if(!strcmp(cmds[i].s, cmd)){
			found = 1;

			if(!child->running && cmds[i].needs_alive)
				printf("child isn't running, can't \"%s\"\n", cmds[i].s);
			else
				cmds[i].f(child), dispatched = 1;
			break;
		}

	if(!found)
		printf("command \"%s\" not found\n", cmd);

	return dispatched;
}

static noreturn void
run_debugger(tracee *child)
{
	for(;;){
		tracee_wait(child);

		switch(child->event){
			case TRACEE_KILLED:
				printf("exited with code %d\n", child->exit_code);
				break;

			case TRACEE_SIGNALED:
				printf("signaled with signal %d\n", child->sig);
				break;

			case TRACEE_TRAPPED:
				printf("trapped\n");
				break;
		}

prompt:
		fputs("(sdb) ", stdout);

		char cmd[64];
		if(!fgets(cmd, sizeof cmd, stdin)){
			putchar('\n');
			c_quit(child);
		}

		char *s;
		if((s = strchr(cmd, '\n')))
			*s = '\0';

		if(!dispatch(child, cmd))
			goto prompt;
	}
}


int
main(int argc, char **argv)
{
	tracee child;

	if(argc < 2){
		fprintf(stderr, "Usage: %s child-args...\n", *argv);
		return 1;
	}

	switch(tracee_create(&child)){
		case 0:
			run_target(argv + 1);
			break;

		default:
			run_debugger(&child);
	}
}
