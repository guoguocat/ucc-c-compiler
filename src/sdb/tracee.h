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
		TRACEE_SIGNALED,
		TRACEE_KILLED
	} event;

		int sig;
		int exit_code;

		bkpt **breakpoints;
} tracee;

void tracee_traceme(void);

pid_t tracee_create(tracee *t);
void  tracee_wait(tracee *t);

void  tracee_kill(tracee *t, int sig);
int   tracee_alive(tracee *t);

void  tracee_continue(tracee *t);
void  tracee_step(tracee *t);

reg_t tracee_read_reg(tracee *t, const char *);

int tracee_break(tracee *t, addr_t);

#endif
