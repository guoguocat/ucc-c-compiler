#include <string.h>

#include "ops.h"
#include "stmt_default.h"
#include "../out/lbl.h"

const char *str_stmt_default()
{
	return "default";
}

void fold_stmt_default(stmt *s)
{
	fold_stmt_and_add_to_curswitch(s, &s->bits.case_lbl);
}

void gen_stmt_default(stmt *s)
{
	out_label(s->bits.case_lbl);
	gen_stmt(s->lhs);
}

void style_stmt_default(stmt *s)
{
	stylef("\ndefault: ");
	gen_stmt(s->lhs);
}

void init_stmt_default(stmt *s)
{
	s->f_passable = label_passable;
}
