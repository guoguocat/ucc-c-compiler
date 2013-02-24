#ifndef TRACEE_H
#define TRACEE_H

#include <sys/types.h>

typedef struct tracee
{
	pid_t pid;
	int running;
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

#endif
