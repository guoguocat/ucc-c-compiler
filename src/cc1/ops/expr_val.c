#include <string.h>

#include "ops.h"

const char *str_expr_val()
{
	return "val";
}

void gen_expr_val_1(expr *e, FILE *f)
{
	asm_out_intval(f, &e->bits.iv);
}

void fold_expr_val(expr *e, symtable *stab)
{
	(void)e;
	(void)stab;
	eof_where = &e->where;
	e->tree_type = decl_new();
	e->tree_type->type->primitive = type_int;
	eof_where = NULL;
}

void gen_expr_val(expr *e, symtable *stab)
{
	(void)stab;

	fputs("\tmov rax, ", cc_out[SECTION_TEXT]);
	e->f_gen_1(e, cc_out[SECTION_TEXT]);
	fputc('\n', cc_out[SECTION_TEXT]);

	asm_temp(1, "push rax");
}

void gen_expr_str_val(expr *e, symtable *stab)
{
	(void)stab;
	idt_printf("val: %ld\n", e->bits.iv.val);
}

void const_expr_val(expr *e, intval *piv, enum constyness *pconst_type)
{
	memcpy(piv, &e->bits.iv, sizeof *piv);
	*pconst_type = CONST_WITH_VAL; /* obviously vals are const */
}

void mutate_expr_val(expr *e)
{
	e->f_gen_1 = gen_expr_val_1;
	e->f_const_fold = const_expr_val;
}

expr *expr_new_val(int val)
{
	expr *e = expr_new_wrapper(val);
	e->bits.iv.val = val;
	return e;
}

void gen_expr_style_val(expr *e, symtable *stab)
{ (void)e; (void)stab; /* TODO */ }
