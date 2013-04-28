#ifndef PTRACE_H
#define PTRACE_H

enum os_pt
{
	SDB_TRACEME,
	SDB_SINGLESTEP,
	SDB_CONT,
	SDB_ATTACH,
	SDB_DETACH,
};

long os_ptrace(enum os_pt, pid_t pid, void *addr, void *data);

#endif
