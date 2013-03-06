#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "util.h"
#include "tracee.h"
#include "prompt.h"

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

static void
c_help(tracee *);

static const struct
{
	const char *s;
	void (*f)(tracee *);
	enum {
		CMD_WAIT_AFTER, CMD_NEEDS_LIVING
	} mode;
} cmds[] = {
	{  "quit",   c_quit,           0                 },
	{  "help",   c_help,           0                 },
	{  "break",  c_break,          0                 },
	{  "x",      c_examine,        CMD_NEEDS_LIVING  },
	{  "kill",   c_kill,           CMD_NEEDS_LIVING | CMD_WAIT_AFTER },
	{  "cont",   tracee_continue,  CMD_NEEDS_LIVING | CMD_WAIT_AFTER },
	{  "step",   tracee_step,      CMD_NEEDS_LIVING | CMD_WAIT_AFTER },

	{ NULL }
};

static void
c_help(tracee *child)
{
	(void)child;

	printf("available commands:\n");
	for(int i = 0; cmds[i].s; i++)
		printf("%s\n", cmds[i].s);
}

static int
dispatch(tracee *child, const char *cmd)
{
	/* TODO: parse cmd */
	int ret = 0, found = 0;

	for(int i = 0; cmds[i].s; i++)
		if(!strcmp(cmds[i].s, cmd)){
			found = 1;

			if(!child->running && cmds[i].mode & CMD_NEEDS_LIVING)
				printf("child isn't running, can't \"%s\"\n", cmds[i].s);
			else
				cmds[i].f(child), ret = cmds[i].mode & CMD_WAIT_AFTER;
			break;
		}

	if(!found)
		printf("command \"%s\" not found\n", cmd);

	return ret;
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

		char *cmd;
		if(!(cmd = prompt())){
			putchar('\n');
			c_quit(child);
		}

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
