#ifndef PTRACE_H
#define PTRACE_H

#include <sys/types.h>
#include <sys/ptrace.h>

long sdb_ptrace(int req, pid_t pid, void *addr, void *data);

#endif
