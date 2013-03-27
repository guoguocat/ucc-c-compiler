#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "util.h"

#include "arch.h"
#include "tracee.h"
#include "cmds.h"

static void
c_kill(tracee *child, char **argv)
{
	tracee_kill(child, SIGKILL);
}

void
c_quit(tracee *child, char **argv)
{
	if(tracee_alive(child))
		c_kill(child, argv);
	exit(0);
}

static void
c_break(tracee *child, char **argv)
{
	/* TODO */
}

static void
c_examine(tracee *child, char **argv)
{
	/* TODO */
}

static void
c_help(tracee *, char **argv);

static void
c_regs_read(tracee *t, char **argv)
{
	const char **r;

	for(r = arch_reg_names(); *r; r++){
		unsigned long v = tracee_read_reg(t, *r);
		printf("%s = 0x%lx\n", *r, v);
	}
}

static void c_cont(tracee *t, char **argv)
{
	tracee_continue(t);
}

static void c_step(tracee *t, char **argv)
{
	tracee_step(t);
}

static const struct
{
	const char *s;
	void (*f)(tracee *, char **);
	enum {
		CMD_WAIT_AFTER   = 1 << 0,
		CMD_NEEDS_LIVING = 1 << 1,
	} mode;
} cmds[] = {
	{  "quit",   c_quit,           0                 },
	{  "help",   c_help,           0                 },
	{  "break",  c_break,          0                 },
	{  "x",      c_examine,        CMD_NEEDS_LIVING  },
	{  "rall",   c_regs_read,      CMD_NEEDS_LIVING  },
	{  "kill",   c_kill,           CMD_NEEDS_LIVING | CMD_WAIT_AFTER },
	{  "cont",   c_cont,           CMD_NEEDS_LIVING | CMD_WAIT_AFTER },
	{  "step",   c_step,           CMD_NEEDS_LIVING | CMD_WAIT_AFTER },

	{ NULL }
};

static void
c_help(tracee *child, char **argv)
{
	(void)child;

	printf("available commands:\n");
	for(int i = 0; cmds[i].s; i++)
		printf("  %s\n", cmds[i].s);
}

int
cmd_dispatch(tracee *child, char **inp)
{
	/* TODO: parse cmd, tab completion, shortened recognition (e.g. "disas") */
	int ret = 0, found = 0;

	for(int i = 0; cmds[i].s; i++)
		if(!strcmp(cmds[i].s, inp[0])){
			found = 1;

			if(cmds[i].mode & CMD_NEEDS_LIVING && !tracee_alive(child))
				printf("child isn't running, can't \"%s\"\n", cmds[i].s);
			else
				cmds[i].f(child, inp), ret = cmds[i].mode & CMD_WAIT_AFTER;
			break;
		}

	if(!found)
		printf("command \"%s\" not found\n", inp[0]);

	return ret;
}
