#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../util/assert.h"
#include "../util/where.h"
#include "../util/util.h"
#include "../util/dynarray.h"
#include "../util/dynmap.h"

#include "basic_blk/bb.h"

#include "data_structs.h"
#include "expr.h"
#include "stmt.h"
#include "blk.h"

#include "stmt_ctx.h"

basic_blk *blockify_expr(expr *e, basic_blk *blk)
{
	basic_blk *r = e->f_block(e, blk);
	UCC_ASSERT(r, "f_block NULL return");
	return r;
}

void blockify_stmt(stmt *s, stmt_fold_ctx_block *ctx)
{
	s->f_block(s, ctx);

	UCC_ASSERT(s->entry && s->exit, "no entry/exit for %s", s->f_str());
}

void blockify_stmt_switch_lbl(stmt *s, stmt_fold_ctx_block *ctx)
{
	blockify_stmt(s, ctx);

	if(!ctx->curswitch)
		die_at(&s->where, "%s not inside switch", s->f_str());

	dynarray_add(&ctx->curswitch->bits.switch_cases, s);
}

static void stmt_fctx_start(stmt_fold_ctx_function *fctx, basic_blk **bb_start, basic_blk **bb_end)
{
	*bb_start = bb_new("fn_start");
	*bb_end   = bb_new("fn_end");

	memset(fctx, 0, sizeof *fctx);
	fctx->gotos = dynmap_new((dynmap_cmp_f *)strcmp);
}

static void stmt_fctx_free(stmt_fold_ctx_function *fctx)
{
	dynmap_free(fctx->gotos);
}

void blockify_func(decl *func)
{
	stmt_fold_ctx_function fctx;
	stmt_fold_ctx_block bctx = { 0 };

	basic_blk *b_start, *b_end;

	bctx.func_ctx = &fctx;

	stmt_fctx_start(&fctx, &b_start, &b_end);

	blockify_stmt(func->func_code, &bctx);

	stmt_fctx_free(&fctx);
}
