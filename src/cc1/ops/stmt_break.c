#include <string.h>

#include "ops.h"
#include "stmt_break.h"
#include "../stmt_ctx.h"

const char *str_stmt_break()
{
	return "break";
}

void fold_stmt_break_continue(stmt *t, basic_blk *jmp)
{
	if(!jmp)
		die_at(&t->where, "%s outside a flow-control statement", t->f_str());

	t->bits.blk_jmp = jmp;
}

void blockify_stmt_break(stmt *t, stmt_fold_ctx_block *ctx)
{
	fold_stmt_break_continue(t, ctx->blk_break);
}

void fold_stmt_break(stmt *t)
{
	(void)t;
}

void mutate_stmt_break(stmt *s)
{
	s->f_passable = fold_passable_yes;
}

STMT_GOTO_DEFS(break);
