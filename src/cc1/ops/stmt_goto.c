#include <stdlib.h>

#include "ops.h"
#include "stmt_goto.h"
#include "../out/lbl.h"
#include "../label.h"

const char *str_stmt_goto()
{
	return "goto";
}

void fold_stmt_goto(stmt *s)
{
	if(!symtab_func(s->symtab))
		die_at(&s->where, "goto outside of a function");

	if(s->expr){
		FOLD_EXPR(s->expr, s->symtab);
	}else{
		(s->bits.lbl.label =
		 symtab_label_find_or_new(
			 s->symtab, s->bits.lbl.spel, &s->where))
			->uses++;
	}
}

void gen_stmt_goto(stmt *s)
{
	if(s->expr)
		gen_expr(s->expr);
	else
		out_push_lbl(s->bits.lbl.label->mangled, 0);

	out_jmp();
}

void style_stmt_goto(stmt *s)
{
	stylef("goto ");

	if(s->expr){
		stylef("*");
		gen_expr(s->expr);
	}else{
		stylef("%s", s->bits.lbl.spel);
	}

	stylef(";");
}

void init_stmt_goto(stmt *s)
{
	s->f_passable = fold_passable_no;
}
