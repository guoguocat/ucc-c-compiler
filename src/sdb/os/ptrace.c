#include "ptrace.h"

#ifdef __APPLE__
#  define DATA_CAST(x) (intptr_t)x
#else
#  define DATA_CAST(x) x
#endif

long sdb_ptrace(int req, pid_t pid, void *addr, void *data)
{
	return ptrace(req, pid, addr, DATA_CAST(data));
}
