#include <string.h>

#include "ops.h"
#include "stmt_case.h"
#include "../out/lbl.h"

const char *str_stmt_case()
{
	return "case";
}

void blockify_stmt_case(stmt *t, stmt_fold_ctx_block *ctx)
{
	//fold_stmt_and_add_to_curswitch(t, ctx);
}

void fold_stmt_case(stmt *t)
{
	FOLD_EXPR(t->expr, t->symtab);
	fold_check_expr(t->expr, FOLD_CHK_INTEGRAL | FOLD_CHK_CONST_I, "case");

	fold_stmt_in_switch(t);
}

void mutate_stmt_case(stmt *s)
{
	s->f_passable = label_passable;
}

STMT_LBL_DEFS(case);
