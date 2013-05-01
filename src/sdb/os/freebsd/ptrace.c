#include <sys/types.h>
#include <sys/ptrace.h>

#include "../ptrace.h"

long os_ptrace(enum os_pt req, pid_t pid, void *addr, void *data)
{
	int r = -1;
	switch(req){
		case SDB_TRACEME: r = PT_TRACE_ME; break;
		case SDB_SINGLESTEP: r = PT_STEP; break;
		case SDB_CONT: r = PT_CONTINUE; break;
		case SDB_ATTACH: r = PT_ATTACH; break;
		case SDB_DETACH: r = PT_DETACH; break;
	}

	return ptrace(r, pid, addr, (intptr_t)data);
}
