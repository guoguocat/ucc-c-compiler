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

int arch_pseudo_reg(enum pseudo_reg r)
{
	switch(r){
		case ARCH_REG_IP: return arch_reg_offset("rip");
		case ARCH_REG_SP: return arch_reg_offset("rsp");
	}
	assert(0 && "bad pseudo reg");
}
