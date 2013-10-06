#include <string.h>

#include "ops.h"
#include "stmt_default.h"
#include "../out/lbl.h"
#include "../blk.h"

const char *str_stmt_default()
{
	return "default";
}

void blockify_stmt_default(stmt *s, stmt_fold_ctx_block *ctx)
{
	blockify_stmt(s->lhs, ctx);

	s->entry = s->lhs->entry;
	s->exit  = s->lhs->exit;
}

void fold_stmt_default(stmt *s)
{
	s->bits.is_default = 1;
	fold_stmt_in_switch(s);
}

void mutate_stmt_default(stmt *s)
{
	s->f_passable = label_passable;
}

STMT_LBL_DEFS(default);
