#include "ops.h"
#include "expr_assign.h"
#include "__builtin.h"

const char *str_expr_assign()
{
	return "assign";
}

int expr_is_lvalue(expr *e)
{
	/*
	 * valid lvaluess:
	 *
	 *   x              = 5; // non-func identifier
	 *   *(expr)        = 5; // dereference
	 *   struct.member  = 5; // struct
	 *   struct->member = 5; // struct
	 *
	 * also can't be const, checked in fold_assign (since we allow const inits)
	 *
	 * order is important
	 */

	/* _lvalue_ addressing makes an exception for this */
	if(type_ref_is(e->tree_type, type_ref_func))
		return 0;

	if(type_ref_is(e->tree_type, type_ref_array))
		return 0;

	if(expr_kind(e, deref))
		return 1;

	if(expr_kind(e, struct))
		return 1;

	if(expr_kind(e, compound_lit))
		return 1;

	if(expr_kind(e, identifier))
		return 1;

	return 0;
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
		/* highest may be -1 - k.bits.num.val is zero */
		const int highest = val_highest_bit(k.bits.num.val.i);
		const int is_signed = type_ref_is_signed(mem->field_width->tree_type);

		const_fold(mem->field_width, &k);

		UCC_ASSERT(k.type == CONST_NUM, "bitfield size not val?");
		UCC_ASSERT(K_FLOATING(k.bits.num), "fp bitfield size?");

		if(highest > (sintegral_t)k.bits.num.val.i
		|| (is_signed && highest == (sintegral_t)k.bits.num.val.i))
		{
			sintegral_t kexp_to = kexp & ~(-1UL << k.bits.num.val.i);

			WARN_AT(&from->where,
					"truncation in store to bitfield alters value: "
					"%" NUMERIC_FMT_D " -> %" NUMERIC_FMT_D,
					kexp, kexp_to);
		}
	}
}

void expr_must_lvalue(expr *e)
{
	if(!expr_is_lvalue(e)){
		DIE_AT(&e->where, "assignment to %s/%s - not an lvalue",
				type_ref_to_str(e->tree_type),
				e->f_str());
	}
}

void fold_expr_assign(expr *e, symtable *stab)
{
	sym *lhs_sym = NULL;

	lhs_sym = fold_inc_writes_if_sym(e->lhs, stab);

	FOLD_EXPR_NO_DECAY(e->lhs, stab);
	FOLD_EXPR(e->rhs, stab);

	if(lhs_sym)
		lhs_sym->nreads--; /* cancel the read that fold_ident thinks it got */

	if(type_ref_is_type(e->rhs->tree_type, type_void))
		DIE_AT(&e->where, "assignment from void expression");

	expr_must_lvalue(e->lhs);

	if(!e->assign_is_init && type_ref_is_const(e->lhs->tree_type))
		DIE_AT(&e->where, "can't modify const expression %s", e->lhs->f_str());

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
		&& (mem = e->lhs->bits.struct_mem.d)->field_width)
		{
			bitfield_trunc_check(mem, e->rhs);
		}
	}


	if(type_ref_is_s_or_u(e->tree_type)){
		e->expr = builtin_new_memcpy(
				e->lhs, e->rhs,
				type_ref_size(e->rhs->tree_type, &e->rhs->where));

		FOLD_EXPR(e->expr, stab);
	}
}

void gen_expr_assign(expr *e)
{
	UCC_ASSERT(!e->assign_is_post, "assign_is_post set for non-compound assign");

	if(type_ref_is_s_or_u(e->tree_type)){
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
