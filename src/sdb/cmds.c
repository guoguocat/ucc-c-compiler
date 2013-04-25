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

#define strcmp_lim(a, k) strncmp(a, k, strlen(a))

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

static int
c_sig(tracee *child, char **argv)
{
	/* e.g. sig TRAP USR1 stop pass
	 *      sig INT send
	 */
	const int argc = ARGC(argv);
	if(argc < 2){
usage:
		warn("Usage: %s send SIG\n"
				 "       %s print|stop|pass... SIG...",
				 *argv, *argv);
		return DISPATCH_REPROMPT;
	}

	if(!strcmp_lim(argv[1], "send")){
		if(argc != 3)
			goto usage;

		int sig = sig_parse(argv[2]);
		if(sig == -1){
			warn("bad signal to send, \"%s\"", argv[2]);
			goto out;
		}

		tracee_kill(child, sig);
		return DISPATCH_WAIT;

	}else{
		handle_mask mask_add = 0, mask_rm = 0;

		int i;
		for(i = 1; i < argc; i++){
			int no;

			no = !strncmp(argv[i], "no", 2);

#define CHECK(str, en)              \
			if(!strcmp_lim(argv[i], str)) \
				*(no ? &mask_rm : &mask_add) |= HANDLE_ ## en

			/**/ CHECK("print", PRINT);
			else CHECK("stop",  STOP);
			else CHECK("pass",  HEREDITARY);
			else break;
#undef CHECK
		}

		if(i == argc){
			warn("no signals to handle");
			goto usage;
		}

		for(; i < argc; i++){
			int sig = sig_parse(argv[i]);
			if(sig == -1){
				warn("couldn't parse \"%s\"", argv[i]);
				goto out;
			}

			sig_handle_mask_set(
					sig,
					(sig_handle_mask(sig) | mask_add) & ~mask_rm);
		}
	}

out:
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
	{ "signal", c_sig,            1 }, /* TODO: lazy */

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
	char *slash = strchr(inp[0], '/');
	const size_t len = slash ? (unsigned)(slash - inp[0]) : strlen(*inp);
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
