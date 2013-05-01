#include <string.h>

#include "ops.h"
#include "stmt_asm.h"
#include "../out/__asm.h"

const char *str_stmt_asm()
{
	return "asm";
}

static void check_constraint(asm_inout *io, symtable *stab, int output)
{
	if(output)
		fold_inc_writes_if_sym(io->exp, stab);

	FOLD_EXPR(io->exp, stab);

	out_constraint_check(&io->exp->where, io->constraints, output);
}

void fold_stmt_asm(stmt *s)
{
	asm_inout **it;
	int n_inouts;

	n_inouts = 0;

	for(it = s->asm_bits->inputs; it && *it; it++, n_inouts++)
		check_constraint(*it, s->symtab, 0);

	for(it = s->asm_bits->outputs; it && *it; it++, n_inouts++){
		asm_inout *io = *it;
		check_constraint(io, s->symtab, 1);
		if(!expr_is_lvalue(io->exp))
			DIE_AT(&io->exp->where, "asm output not an lvalue");
	}

	/* validate asm string - s->asm_bits->cmd */
	if(s->asm_bits->extended){
		char *str;

		for(str = s->asm_bits->cmd; *str; str++)
			if(*str == '%'){
				if(str[1] == '%'){
					str++;

				}else if(str[1] == '['){
					ICE("TODO: named constraint");

				}else{
					int pos;

					if(sscanf(str + 1, "%d", &pos) != 1)
						DIE_AT(&s->where, "invalid register character '%c', number expected", str[1]);

					if(pos >= n_inouts)
						DIE_AT(&s->where, "invalid register index %d / %d", pos, n_inouts);
				}
			}
	}
}

void gen_stmt_asm(stmt *s)
{
	asm_inout **ios;
	int npops = 0;
	int i;

	for(ios = s->asm_bits->outputs, i = 0; ios && ios[i]; i++, npops++){
		asm_inout *const io = ios[i];

		lea_expr(io->exp, s->symtab);
	}

	if((ios = s->asm_bits->inputs)){
		for(i = 0; ios && ios[i]; i++, npops++)
			gen_expr(ios[i]->exp, s->symtab);

		/* move into the registers or wherever necessary */
		for(i--; i >= 0; i--)
			out_constrain(ios[i]);
	}

	out_comment("### begin asm(%s) from %s",
			s->asm_bits->extended ? ":::" : "",
			where_str(&s->where));

	out_asm_inline(s->asm_bits);

	out_comment("### end asm()");

	while(npops --> 0)
		out_pop();
}

void mutate_stmt_asm(stmt *s)
{
	s->f_passable = fold_passable_yes;
}
