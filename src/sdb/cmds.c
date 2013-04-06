#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "util.h"
#include "../util/dynarray.h"
#define ARGC(argv) dynarray_count((void **)argv)

#include "arch.h"
#include "tracee.h"
#include "cmds.h"
#include "signal.h"

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
	if(ARGC(argv) > 2){
		warn("Usage: %s [addr]", *argv);
		return;
	}

	addr_t addr;
	if(argv[1]){
		if(sscanf(argv[1], "0x%lx", &addr) != 1){
			warn("%s isn't an address", argv[1]);
			return;
		}
	}else if(tracee_get_reg(child, ARCH_REG_IP, &addr)){
		warn("read register ip:");
		return;
	}

	if(tracee_break(child, addr))
		warn("break:");
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
		int i = arch_reg_offset(*r);

		assert(i != -1);

		reg_t v;
		if(arch_reg_read(t->pid, i, &v))
			warn("read reg \"%s\":", *r);
		else
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

static void c_reg_read(tracee *t, char **argv)
{
	if(ARGC(argv) != 2){
		warn("Usage: %s register", *argv);
		return;
	}

	int i = arch_reg_offset(argv[1]);
	if(i == -1){
		warn("unknown register \"%s\"", argv[1]);
		return;
	}

	reg_t v;
	if(arch_reg_read(t->pid, i, &v)){
		warn("reg read:");
		return;
	}

	printf("0x%lx\n", v);
}

static void c_reg_write(tracee *t, char **argv)
{
	if(ARGC(argv) != 3){
		warn("Usage: %s register value", *argv);
		return;
	}

	int i = arch_reg_offset(argv[1]);
	if(i == -1){
		warn("unknown register \"%s\"", argv[1]);
		return;
	}

	reg_t v;
	if(sscanf(argv[2], "%lu", &v) != 1){
		warn("%s isn't a register value", argv[2]);
		return;
	}

	if(arch_reg_write(t->pid, i, v))
		warn("reg write:");
}

static const struct dispatch
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
	{  "rr"  ,   c_reg_read,       CMD_NEEDS_LIVING  },
	{  "rw"  ,   c_reg_write,      CMD_NEEDS_LIVING  },
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
	enum { DISPATCH_REPROMPT = 0, DISPATCH_WAIT = 1 };

	/* TODO: parse cmd, tab completion, shortened recognition (e.g. "disas") */
	const size_t len = strlen(*inp);
	const struct dispatch *found = NULL;
	int ret = DISPATCH_REPROMPT;

	for(int i = 0; cmds[i].s; i++)
		if(!strncmp(cmds[i].s, inp[0], len)){
			if(found){
				warn("ambiguous command \"%s\"", inp[0]);
				return DISPATCH_REPROMPT;
			}
			found = &cmds[i];
		}

	if(found){
		// FIXME: remove alive
		if(found->mode & CMD_NEEDS_LIVING && !tracee_alive(child))
			printf("child isn't running, can't \"%s\"\n", found->s);
		else
			found->f(child, inp), ret = found->mode & CMD_WAIT_AFTER;
	}else{
		printf("command \"%s\" not found\n", inp[0]);
	}

	return ret;
}
