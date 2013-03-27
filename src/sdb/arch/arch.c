#include <stdlib.h>
#include <string.h>

#include "../arch.h"

const char **arch_reg_names(void)
{
	static const char *regs[] = {
#define REG(r) #r,
#include "regs.def"
#undef REG
		NULL
	};

	return regs;
}
