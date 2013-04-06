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

enum { DISPATCH_REPROMPT = 0, DISPATCH_WAIT = 1 };

static int
c_kill(tracee *child, char **argv)
{
	tracee_kill(child, SIGKILL);
	return DISPATCH_WAIT;
}

int
c_quit(tracee *child, char **argv)
{
	if(tracee_alive(child))
		c_kill(child, argv);
	exit(0);
}

static int
c_break(tracee *child, char **argv)
{
	if(ARGC(argv) > 2){
		warn("Usage: %s [addr]", *argv);
		return DISPATCH_REPROMPT;
	}

	addr_t addr;
	if(argv[1]){
		if(sscanf(argv[1], "0x%lx", &addr) != 1){
			warn("%s isn't an address", argv[1]);
			return DISPATCH_REPROMPT;
		}
	}else if(tracee_get_reg(child, ARCH_REG_IP, &addr)){
		warn("read register ip:");
		return DISPATCH_REPROMPT;
	}

	if(tracee_break(child, addr))
		warn("break:");

	return DISPATCH_REPROMPT;
}

static int
c_examine(tracee *child, char **argv)
{
	/* TODO */
	return DISPATCH_REPROMPT;
}

static int
c_help(tracee *, char **argv);

static int
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

	return DISPATCH_REPROMPT;
}

static int c_cont(tracee *t, char **argv)
{
	tracee_continue(t);

	return DISPATCH_WAIT;
}

static int c_step(tracee *t, char **argv)
{
	tracee_step(t);

	return DISPATCH_WAIT;
}

static int c_reg_read(tracee *t, char **argv)
{
	if(ARGC(argv) != 2){
		warn("Usage: %s register", *argv);
		return DISPATCH_REPROMPT;
	}

	int i = arch_reg_offset(argv[1]);
	if(i == -1){
		warn("unknown register \"%s\"", argv[1]);
		return DISPATCH_REPROMPT;
	}

	reg_t v;
	if(arch_reg_read(t->pid, i, &v)){
		warn("reg read:");
		return DISPATCH_REPROMPT;
	}

	printf("0x%lx\n", v);

	return DISPATCH_REPROMPT;
}

static int c_reg_write(tracee *t, char **argv)
{
	if(ARGC(argv) != 3){
		warn("Usage: %s register value", *argv);
		return DISPATCH_REPROMPT;
	}

	int i = arch_reg_offset(argv[1]);
	if(i == -1){
		warn("unknown register \"%s\"", argv[1]);
		return DISPATCH_REPROMPT;
	}

	reg_t v;
	if(sscanf(argv[2], "%lu", &v) != 1){
		warn("%s isn't a register value", argv[2]);
		return DISPATCH_REPROMPT;
	}

	if(arch_reg_write(t->pid, i, v))
		warn("reg write:");

	return DISPATCH_REPROMPT;
}

static const struct dispatch
{
	const char *s;
	int (*f)(tracee *, char **);
	int need_alive;
} cmds[] = {
	{ "quit",   c_quit,           0 },
	{ "help",   c_help,           0 },
	{ "break",  c_break,          1 }, /* TODO: lazy */
	{ "x",      c_examine,        1 },
	{ "rall",   c_regs_read,      1 },
	{ "rr"  ,   c_reg_read,       1 },
	{ "rw"  ,   c_reg_write,      1 },
	{ "kill",   c_kill,           1 },
	{ "cont",   c_cont,           1 },
	{ "step",   c_step,           1 },

	{ NULL }
};

static int
c_help(tracee *child, char **argv)
{
	printf("available commands:\n");
	for(int i = 0; cmds[i].s; i++)
		printf("  %s\n", cmds[i].s);

	return DISPATCH_REPROMPT;
}

int
cmd_dispatch(tracee *child, char **inp)
{
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
		if(found->need_alive && !tracee_alive(child))
			printf("child isn't running, can't \"%s\"\n", found->s);
		else
			ret = found->f(child, inp);
	}else{
		printf("command \"%s\" not found\n", inp[0]);
	}

	return ret;
}
