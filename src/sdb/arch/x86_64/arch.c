#include <assert.h>
#include "../../arch.h"

unsigned long arch_inst_trap()
{
	return 0xCC; /* int3 */
}
