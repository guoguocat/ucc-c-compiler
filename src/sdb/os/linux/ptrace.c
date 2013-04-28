#include <sys/types.h>
#include <sys/ptrace.h>

#include "../ptrace.h"

long os_ptrace(enum os_pt req, pid_t pid, void *addr, void *data)
{
	int r = -1;

	switch(req){
#define MAP(x) case SDB_ ## x: r = PTRACE_ ## x; break
		MAP(TRACEME);
		MAP(SINGLESTEP);
		MAP(CONT);
		MAP(ATTACH);
		MAP(DETACH);
	}

	return ptrace(r, pid, addr, data);
}
