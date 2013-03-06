#ifndef PTRACE_H
#define PTRACE_H

#include <sys/types.h>
#include <sys/ptrace.h>

#ifdef __APPLE__
#  define PTRACE_TRACEME    PT_TRACE_ME
#  define PTRACE_SINGLESTEP PT_STEP
#  define PTRACE_CONT       PT_CONTINUE
#else
#endif

long sdb_ptrace(int req, pid_t pid, void *addr, void *data);

#endif
