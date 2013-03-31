#ifndef TRACEE_H
#define TRACEE_H

#include <sys/types.h>
#include "breakpoint.h"

typedef struct tracee
{
	pid_t pid;
	enum
	{
		TRACEE_TRAPPED,
		TRACEE_BREAK,
		TRACEE_SIGNALED,
		TRACEE_KILLED,
	} event;

	union
	{
		int sig;
		int exit_code;
		bkpt *bkpt;
	} evt;

	bkpt **bkpts;
} tracee;

void tracee_traceme(void);

pid_t tracee_create(tracee *t);
void  tracee_wait(tracee *t);

void  tracee_kill(tracee *t, int sig);
int   tracee_alive(tracee *t);

void  tracee_continue(tracee *t);
void  tracee_step(tracee *t);

int tracee_break(tracee *t, addr_t);

int tracee_get_reg(tracee *t, enum pseudo_reg r, reg_t *p);
int tracee_set_reg(tracee *t, enum pseudo_reg r, const reg_t v);

#endif
