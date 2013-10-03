#include <string.h>

#include "ops.h"
#include "stmt_default.h"
#include "../out/lbl.h"

const char *str_stmt_default()
{
	return "default";
}

void fold_stmt_default(stmt *s, stmt_fold_ctx_block *ctx)
{
	s->bits.is_default = 1;
	fold_stmt_and_add_to_curswitch(s, ctx);
}

void mutate_stmt_default(stmt *s)
{
	s->f_passable = label_passable;
}

STMT_LBL_DEFS(default);
