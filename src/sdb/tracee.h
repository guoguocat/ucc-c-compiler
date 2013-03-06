#ifndef TRACEE_H
#define TRACEE_H

#include <sys/types.h>
#include "arch.h"

typedef struct tracee
{
	pid_t pid;
	enum
	{
		TRACEE_TRAPPED,
		TRACEE_SIGNALED,
		TRACEE_KILLED
	} event;

	union
	{
		int sig;
		int exit_code;
	};
} tracee;

void tracee_traceme(void);

pid_t tracee_create(tracee *t);
void  tracee_wait(tracee *t);

void  tracee_kill(tracee *t, int sig);
int   tracee_alive(tracee *t);

void  tracee_continue(tracee *t);
void  tracee_step(tracee *t);

void tracee_read_regs(tracee *t, struct arch_regs *);

#endif
