#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "../util/util.h"
#include "../util/alloc.h"
#include "data_structs.h"
#include "cc1.h"
#include "const.h"

/* needed for expr_assignment() */
#include "ops/expr_assign.h"

void expr_mutate(expr *e, func_mutate_expr *f,
		func_fold *f_fold,
		func_str *f_str,
		func_gen *f_gen,
		func_gen *f_gen_str,
		func_gen *f_gen_style
		)
{
	e->f_fold = f_fold;
	e->f_str  = f_str;

	switch(cc1_backend){
		case BACKEND_ASM:   e->f_gen = f_gen;       break;
		case BACKEND_PRINT: e->f_gen = f_gen_str;   break;
		case BACKEND_STYLE: e->f_gen = f_gen_style; break;
		default: ICE("bad backend");
	}

	e->f_const_fold = NULL;
	e->f_lea = NULL;

	f(e);
}

expr *expr_new(func_mutate_expr *f,
		func_fold *f_fold,
		func_str *f_str,
		func_gen *f_gen,
		func_gen *f_gen_str,
		func_gen *f_gen_style)
{
	expr *e = umalloc(sizeof *e);
	where_cc1_current(&e->where);
	expr_mutate(e, f, f_fold, f_str, f_gen, f_gen_str, f_gen_style);
	return e;
}

expr *expr_set_where(expr *e, where const *w)
{
	memcpy_safe(&e->where, w);
	return e;
}

expr *expr_set_where_len(expr *e, where *start)
{
	where end;
	where_cc1_current(&end);

	expr_set_where(e, start);
	if(start->line == end.line)
		e->where.len = end.chr - start->chr;

	return e;
}

expr *expr_new_intval(intval *iv)
{
	expr *e = expr_new_val(0);
	memcpy_safe(&e->bits.iv, iv);
	return e;
}

expr *expr_new_decl_init(decl *d, decl_init *di)
{
	ICE("TODO - only allow simple expr inits");
	(void)d;
	(void)di;
	/*UCC_ASSERT(d->init, "no init");
	return expr_new_assign_init(expr_new_identifier(d->spel), d->init);*/
	return 0;
}

#if 0
expr *expr_new_array_decl_init(decl *d, int ival, int idx)
{
	expr *sum;

	UCC_ASSERT(d->init, "no init");

	sum = expr_new_op(op_plus);

	sum->lhs = expr_new_identifier(d->spel);
	sum->rhs = expr_new_val(idx);

	return expr_new_assign(expr_new_deref(sum), expr_new_val(ival));
}
#endif

int expr_is_null_ptr(expr *e, int allow_int)
{
	int b = 0;

	if(type_ref_is_type(type_ref_is_ptr(e->tree_type), type_void))
		b = 1;
	else if(allow_int && type_ref_is_integral(e->tree_type))
		b = 1;

	return b && const_expr_and_zero(e);
}

int expr_is_lval(expr *e)
{
	if(!e->f_lea)
		return 0;

	/* special case:
	 * (a = b) = c
	 * ^~~~~~~ not an lvalue, but internally we handle it as one
	 */
	if(expr_kind(e, assign) && type_ref_is_s_or_u(e->tree_type))
		return 0;

	if(type_ref_is_array(e->tree_type))
		return 0;

	return 1;
}

expr *expr_new_array_idx_e(expr *base, expr *idx)
{
	expr *op = expr_new_op(op_plus);
	op->lhs = base;
	op->rhs = idx;
	return expr_new_deref(op);
}

expr *expr_new_array_idx(expr *base, int i)
{
	return expr_new_array_idx_e(base, expr_new_val(i));
}

expr *expr_skip_casts(expr *e)
{
	while(expr_kind(e, cast))
		e = e->expr;
	return e;
}
