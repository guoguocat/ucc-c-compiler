#include "ops.h"

#define SIZEOF_WHAT(e) ((e)->expr ? (e)->expr->tree_type : (e)->decl)
#define SIZEOF_SIZE(e)  (e)->val.iv.val

const char *str_expr_sizeof()
{
	return "sizeof";
}

void fold_expr_sizeof(expr *e, symtable *stab)
{
	decl *chosen;

	if(e->expr)
		fold_expr(e->expr, stab);

	chosen = SIZEOF_WHAT(e);

	if(decl_has_incomplete_array(chosen))
		DIE_AT(&e->where, "sizeof incomplete array");

	SIZEOF_SIZE(e) = decl_size(SIZEOF_WHAT(e));

	e->tree_type = decl_new();
	/* size_t */
	e->tree_type->type->primitive = type_int;
	e->tree_type->type->is_signed = 0;
}

int const_expr_sizeof(expr *e)
{
	return e->tree_type ? 0 : 1; /* constant, once folded */
}

void gen_expr_sizeof_1(expr *e)
{
	ICE("TODO: init with %s", e->f_str());
}

void gen_expr_sizeof(expr *e, symtable *stab)
{
	decl *d = SIZEOF_WHAT(e);
	(void)stab;

	asm_temp(1, "push %d ; sizeof %s%s",
			e->val.iv.val,
			e->expr ? "" : "type ",
			decl_to_str(d));
}

void gen_expr_str_sizeof(expr *e, symtable *stab)
{
	(void)stab;
	if(e->expr){
		idt_printf("sizeof expr:\n");
		print_expr(e->expr);
	}else{
		idt_printf("sizeof %s\n", decl_to_str(e->decl));
	}
	idt_printf("size = %ld\n", SIZEOF_SIZE(e));
}

void mutate_expr_sizeof(expr *e)
{
	e->f_const_fold = const_expr_sizeof;
}

expr *expr_new_sizeof_decl(decl *d)
{
	expr *e = expr_new_wrapper(sizeof);
	e->decl = d;
	return e;
}

expr *expr_new_sizeof_expr(expr *sizeof_this)
{
	expr *e = expr_new_wrapper(sizeof);
	e->expr = sizeof_this;
	return e;
}

void gen_expr_style_sizeof(expr *e, symtable *stab)
{ (void)e; (void)stab; /* TODO */ }
