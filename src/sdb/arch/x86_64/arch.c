#include <assert.h>
#include "../../arch.h"

unsigned long arch_trap_mask()
{
	return ~0xfful;
}

unsigned long arch_trap_inst()
{
	return 0xCC; /* int3 */
}

unsigned long arch_trap_size()
{
	return sizeof(char);
}
