#include <string.h>

#include "ops.h"
#include "stmt_case.h"
#include "../out/lbl.h"

const char *str_stmt_case()
{
	return "case";
}

void fold_stmt_case(stmt *t)
{
	intval_t val;

	FOLD_EXPR(t->expr, t->symtab);
	fold_need_expr(t->expr, "case", 0);
	val = const_fold_val(t->expr);

	t->expr->bits.ident.spel = out_label_case(CASE_CASE, val);

	fold_stmt_and_add_to_curswitch(t);
}

void gen_stmt_case(stmt *s)
{
	out_label(s->expr->bits.ident.spel);
	gen_stmt(s->lhs);
}

void style_stmt_case(stmt *s)
{
	stylef("\ncase %ld: ", (long)const_fold_val(s->expr));
	gen_stmt(s->lhs);
}

void init_stmt_case(stmt *s)
{
	s->f_passable = label_passable;
}
