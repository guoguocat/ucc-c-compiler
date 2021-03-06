#include "ops.h"
#include "expr_assign.h"
#include "__builtin.h"
#include "../type_is.h"

const char *str_expr_assign()
{
	return "assign";
}

void bitfield_trunc_check(decl *mem, expr *from)
{
	consty k;

	if(expr_kind(from, cast)){
		/* we'll warn about bitfield truncation, prevent warnings
		 * about cast truncation
		 */
		from->expr_cast_implicit = 0;
	}

	const_fold(from, &k);

	if(k.type == CONST_NUM){
		const sintegral_t kexp = k.bits.num.val.i;
		/* highest may be -1 - kexp is zero */
		const int highest = integral_high_bit(k.bits.num.val.i, from->tree_type);
		const int is_signed = type_is_signed(mem->bits.var.field_width->tree_type);

		const_fold(mem->bits.var.field_width, &k);

		UCC_ASSERT(k.type == CONST_NUM, "bitfield size not val?");
		UCC_ASSERT(K_INTEGRAL(k.bits.num), "fp bitfield size?");

		if(highest > (sintegral_t)k.bits.num.val.i
		|| (is_signed && highest == (sintegral_t)k.bits.num.val.i))
		{
			sintegral_t kexp_to = kexp & ~(-1UL << k.bits.num.val.i);

			warn_at(&from->where,
					"truncation in store to bitfield alters value: "
					"%" NUMERIC_FMT_D " -> %" NUMERIC_FMT_D,
					kexp, kexp_to);
		}
	}
}

void expr_must_lvalue(expr *e)
{
	if(!expr_is_lval(e)){
		die_at(&e->where, "assignment to %s/%s - not an lvalue",
				type_to_str(e->tree_type),
				e->f_str());
	}
}

static void lea_assign_lhs(expr *e)
{
	/* generate our assignment, then lea
	 * our lhs, i.e. the struct identifier
	 * we're assigning to */
	gen_expr(e);
	out_pop();
	lea_expr(e->lhs);
}

void expr_assign_const_check(expr *e, where *w)
{
	if(type_is_const(e->tree_type)){
		fold_had_error = 1;
		warn_at_print_error(w, "can't modify const expression %s",
				e->f_str());
	}
}

void fold_expr_assign(expr *e, symtable *stab)
{
	sym *lhs_sym = NULL;

	lhs_sym = fold_inc_writes_if_sym(e->lhs, stab);

	fold_expr_no_decay(e->lhs, stab);
	FOLD_EXPR(e->rhs, stab);

	if(lhs_sym)
		lhs_sym->nreads--; /* cancel the read that fold_ident thinks it got */

	if(type_is_primitive(e->rhs->tree_type, type_void))
		die_at(&e->where, "assignment from void expression");

	expr_must_lvalue(e->lhs);

	if(!e->assign_is_init)
		expr_assign_const_check(e->lhs, &e->where);

	fold_check_restrict(e->lhs, e->rhs, "assignment", &e->where);

	e->tree_type = e->lhs->tree_type;

	/* type check */
	fold_type_chk_and_cast(
			e->lhs->tree_type, &e->rhs,
			stab, &e->where, "assignment");

	/* the only way to get a value into a bitfield (aside from memcpy / indirection) is via this
	 * hence we're fine doing the truncation check here
	 */
	{
		decl *mem;
		if(expr_kind(e->lhs, struct)
		&& (mem = e->lhs->bits.struct_mem.d)->bits.var.field_width)
		{
			bitfield_trunc_check(mem, e->rhs);
		}
	}


	if(type_is_s_or_u(e->tree_type)){
		e->expr = builtin_new_memcpy(
				e->lhs, e->rhs,
				type_size(e->rhs->tree_type, &e->rhs->where));

		FOLD_EXPR(e->expr, stab);

		/* set f_lea, so we can participate in struct-copy chains
		 * FIXME: don't interpret as an lvalue, e.g. (a = b) = c;
		 * this is currently special cased in expr_is_lval()
		 */
		e->f_lea = lea_assign_lhs;

	}
}

void gen_expr_assign(expr *e)
{
	UCC_ASSERT(!e->assign_is_post, "assign_is_post set for non-compound assign");

	if(type_is_s_or_u(e->tree_type)){
		/* memcpy */
		gen_expr(e->expr);
	}else{
		/* optimisation: do this first, since rhs might also be a store */
		gen_expr(e->rhs);
		lea_expr(e->lhs);
		out_swap();

		out_store();
	}
}

void gen_expr_str_assign(expr *e)
{
	idt_printf("assignment, expr:\n");
	idt_printf("assign to:\n");
	gen_str_indent++;
	print_expr(e->lhs);
	gen_str_indent--;
	idt_printf("assign from:\n");
	gen_str_indent++;
	print_expr(e->rhs);
	gen_str_indent--;
}

void mutate_expr_assign(expr *e)
{
	e->freestanding = 1;
}

expr *expr_new_assign(expr *to, expr *from)
{
	expr *ass = expr_new_wrapper(assign);

	ass->lhs = to;
	ass->rhs = from;

	return ass;
}

expr *expr_new_assign_init(expr *to, expr *from)
{
	expr *e = expr_new_assign(to, from);
	e->assign_is_init = 1;
	return e;
}

void gen_expr_style_assign(expr *e)
{
	gen_expr(e->lhs);
	stylef(" = ");
	gen_expr(e->rhs);
}
