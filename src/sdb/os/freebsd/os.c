#include <errno.h>

#include "../../arch.h"

int arch_read(pid_t pid, addr_t addr, void *p, size_t l)
{
	errno = ENOSYS;
	return -1;
}

int arch_write(pid_t pid, addr_t addr, const void *p, size_t l)
{
	errno = ENOSYS;
	return -1;
}

int arch_reg_read(pid_t pid, enum arch_reg r, reg_t *p)
{
	errno = ENOSYS;
	return -1;
}

int arch_reg_write(pid_t pid, enum arch_reg r, const reg_t v)
{
	// TODO
	errno = ENOSYS;
	return -1;
}
