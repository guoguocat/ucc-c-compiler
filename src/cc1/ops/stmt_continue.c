#include "ops.h"
#include "stmt_continue.h"
#include "stmt_break.h"

#include "../stmt_ctx.h"

const char *str_stmt_continue()
{
	return "continue";
}

void fold_stmt_continue(stmt *t, stmt_fold_ctx_block *ctx)
{
	fold_stmt_break_continue(t, ctx->blk_continue);
}

void mutate_stmt_continue(stmt *s)
{
	s->f_passable = fold_passable_no;
}

STMT_GOTO_DEFS(continue);
