#include <stdlib.h>

#include "ops.h"
#include "stmt_goto.h"
#include "../out/lbl.h"

#include "../stmt_ctx.h"
#include "../../util/dynmap.h"

const char *str_stmt_goto()
{
	return "goto";
}

void blockify_stmt_goto(stmt *s, stmt_fold_ctx_block *ctx)
{
	basic_blk *target = dynmap_get(
			char *, basic_blk *,
			ctx->func_ctx->gotos, s->bits.goto_.lbl);

	if(!target)
		die_at(&s->where, "goto label \"%s\" not found", s->bits.goto_.lbl);

	s->bits.goto_.blk = target;
}

void fold_stmt_goto(stmt *s)
{
	if(s->expr)
		FOLD_EXPR(s->expr, s->symtab);
}

basic_blk *gen_stmt_goto(stmt *s, basic_blk *bb)
{
	if(s->expr)
		bb = gen_expr(s->expr, bb);
	else
		out_push_lbl(bb, s->expr->bits.ident.spel, 0);

	ICE("TODO: goto");

	return bb;
}

basic_blk *style_stmt_goto(stmt *s, basic_blk *bb)
{
	stylef("goto ");

	if(s->expr){
		stylef("*");
		bb = gen_expr(s->expr, bb);
	}else{
		stylef("%s", s->expr->bits.ident.spel);
	}

	stylef(";");

	return bb;
}

void mutate_stmt_goto(stmt *s)
{
	s->f_passable = fold_passable_no;
}
