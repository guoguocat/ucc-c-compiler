#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "../../util/util.h"
#include "../../util/dynarray.h"
#include "../../util/platform.h"
#include "../expr.h"
#include "__builtin.h"
#include "__builtin_va.h"

#include "../cc1.h"
#include "../fold.h"
#include "../gen_asm.h"
#include "../out/out.h"
#include "../out/lbl.h"
#include "../pack.h"
#include "../sue.h"
#include "../funcargs.h"

#include "__builtin_va.h"

#include "../tokenise.h"
#include "../tokconv.h"
#include "../parse_type.h"
#include "../type_is.h"
#include "../type_nav.h"

#include "../parse_expr.h"

static void va_type_check(expr *va_l, expr *in, symtable *stab)
{
	/* we need to check decayed, since we may have
	 * f(va_list l)
	 * aka
	 * f(__builtin_va_list *l) [the array has decayed]
	 */
	enum type_cmp cmp;

	if(!symtab_func(stab))
		die_at(&in->where, "%s() outside a function",
				BUILTIN_SPEL(in));

	cmp = type_cmp(va_l->tree_type,
			type_decay(type_nav_va_list(cc1_type_nav, stab)), 0);

	if(!(cmp & TYPE_EQUAL_ANY)){
		die_at(&va_l->where,
				"first argument to %s should be a va_list (not %s)",
				BUILTIN_SPEL(in), type_to_str(va_l->tree_type));
	}
}

static void va_ensure_variadic(expr *e, symtable *stab)
{
	funcargs *args = type_funcargs(symtab_func(stab)->ref);

	if(!args->variadic)
		die_at(&e->where, "%s in non-variadic function", BUILTIN_SPEL(e->expr));
}

static void fold_va_start(expr *e, symtable *stab)
{
	expr *va_l;

	if(dynarray_count(e->funcargs) != 2)
		die_at(&e->where, "%s requires two arguments", BUILTIN_SPEL(e->expr));

	va_l = e->funcargs[0];
	fold_inc_writes_if_sym(va_l, stab);

	FOLD_EXPR(e->funcargs[0], stab);
	FOLD_EXPR(e->funcargs[1], stab);

	va_l = e->funcargs[0];
	va_type_check(va_l, e->expr, stab);

	va_ensure_variadic(e, stab);

	/* second arg check */
	{
		sym *second = NULL;
		decl **args = symtab_func_root(stab)->decls;
		sym *arg = args[dynarray_count(args) - 1]->sym;
		expr *last_exp = expr_skip_casts(e->funcargs[1]);

		if(expr_kind(last_exp, identifier))
			second = last_exp->bits.ident.sym;

		if(second != arg)
			warn_at(&last_exp->where,
					"second parameter to va_start "
					"isn't last named argument");
	}

#ifndef UCC_VA_ABI
	{
		stmt *assigns = stmt_set_where(
				stmt_new_wrapper(code, symtab_new(stab, &e->where)),
				&e->where);
		expr *assign;

#define W(exp) expr_set_where((exp), &e->where)

#define ADD_ASSIGN(memb, exp)                     \
		assign = W(expr_new_assign(                   \
		        W(expr_new_struct(                    \
		          va_l, 0 /* ->  since it's [1] */,   \
		            W(expr_new_identifier(memb)))),   \
		        exp));                                \
                                                  \
		      dynarray_add(&assigns->bits.code.stmts, \
		        stmt_set_where(                       \
		          expr_to_stmt(assign, stab),         \
		          &e->where))

#define ADD_ASSIGN_VAL(memb, val) ADD_ASSIGN(memb, W(expr_new_val(val)))

		const int ws = platform_word_size();
		struct
		{
			unsigned gp, fp;
		} nargs = { 0, 0 };
		funcargs *const fa = type_funcargs(symtab_func(stab)->ref);

		funcargs_ty_calc(fa, &nargs.gp, &nargs.fp);

		/* need to set the offsets to act as if we've skipped over
		 * n call regs, since we may already have some arguments used
		 */
		ADD_ASSIGN_VAL("gp_offset", nargs.gp * ws);
		ADD_ASSIGN_VAL("fp_offset", (6 + nargs.fp) * ws);
		/* FIXME: x86_64::N_CALL_REGS_I reference above */

		/* adjust to take the skip into account */
		ADD_ASSIGN("reg_save_area",
				W(expr_new_op2(op_minus,
					W(builtin_new_reg_save_area()),
					/* void arith - need _pws
					 * total arg count * ws */
					W(expr_new_val((nargs.gp + nargs.fp) * ws)))));

		ADD_ASSIGN("overflow_arg_area",
				W(expr_new_op2(op_plus,
					W(builtin_new_frame_address(0)),
					/* *2 to step over saved-rbp and saved-ret */
					W(expr_new_val(ws * 2)))));


		fold_stmt(assigns);
		e->bits.variadic_setup = assigns;
	}
#undef ADD_ASSIGN
#undef ADD_ASSIGN_VAL
#undef W
#endif

	e->tree_type = type_nav_btype(cc1_type_nav, type_void);
}

static void builtin_gen_va_start(expr *e)
{
#ifdef UCC_VA_ABI
	/*
	 * va_list is 8-bytes. use the second 4 as the int
	 * offset into saved_reg_args. if this is >= N_CALL_REGS,
	 * we go into saved_var_args (see out/out.c::push_sym sym_arg
	 * for reference)
	 *
	 * assign to
	 *   e->funcargs[0]
	 * from
	 *   0L
	 */
	lea_expr(e->funcargs[0], stab);
	out_push_zero(type_new_INTPTR_T());
	out_store();
#else
	out_comment("va_start() begin");
	gen_stmt(e->bits.variadic_setup);
	out_push_noop();
	out_comment("va_start() end");
#endif
}

expr *parse_va_start(const char *ident, symtable *scope)
{
	/* va_start(__builtin_va_list &, identifier)
	 * second argument may be any expression - we don't use it
	 */
	expr *fcall = parse_any_args(scope);
	(void)ident;
	expr_mutate_builtin_gen(fcall, va_start);
	return fcall;
}

static void va_arg_gen_read(
		expr *const e,
		type *const ty,
		decl *const offset_decl, /* varies - float or integral */
		decl *const mem_reg_save_area,
		decl *const mem_overflow_arg_area)
{
	char *lbl_stack = out_label_code("va_else");
	char *lbl_fin   = out_label_code("va_fin");
	char vphi_buf[OUT_VPHI_SZ];

	/* FIXME: this needs to reference x86_64::N_CALL_REGS_{I,F} */
	const int fp = type_is_floating(ty);
	const unsigned max_reg_args_sz = 6 * 8 + (fp ? 16 * 16 : 0);
	const unsigned ws = platform_word_size();
	const unsigned increment = fp ? 2 * ws : ws;

	gen_expr(e->lhs); /* va_list */
	out_change_type(type_ptr_to(type_nav_btype(cc1_type_nav, type_void)));
	out_dup(); /* va, va */

	out_push_l(
			type_nav_btype(cc1_type_nav, type_long),
			offset_decl->bits.var.struct_offset);

	out_op(op_plus); /* va, &va.gp_offset */

	/*out_set_lvalue(); * val.gp_offset is an lvalue */

	out_change_type(type_ptr_to(type_nav_btype(cc1_type_nav, type_int)));
	out_dup(); /* va, &gp_o, &gp_o */

	out_deref(); /* va, &gp_o, gp_o */
	out_push_l(type_nav_btype(cc1_type_nav, type_int), max_reg_args_sz);
	out_op(op_lt); /* va, &gp_o, <cond> */
	out_jfalse(lbl_stack);

	/* register code */
	out_dup(); /* va, &gp_o, &gp_o */
	out_deref(); /* va, &gp_o, gp_o */

	/* increment either 8 for an integral, or 16 for a float argument
	 * since xmm0 are 128-bit registers, aka 16 byte
	 */
	out_push_l(type_nav_btype(cc1_type_nav, type_int), increment); /* pws */
	out_op(op_plus); /* va, &gp_o, gp_o+ws */

	out_store(); /* va, gp_o+ws */
	out_push_l(type_nav_btype(cc1_type_nav, type_int), increment); /* pws */
	out_op(op_minus); /* va, gp_o */
	out_change_type(type_nav_btype(cc1_type_nav, type_long));

	out_swap(); /* gp_o, va */
	out_push_l(
			type_nav_btype(cc1_type_nav, type_long),
			mem_reg_save_area->bits.var.struct_offset);

	out_op(op_plus); /* gp_o, &reg_save_area */
	out_change_type(type_ptr_to(type_nav_btype(cc1_type_nav, type_long)));
	out_deref();
	out_swap();
	out_op(op_plus); /* reg_save_area + gp_o */

	out_push_lbl(lbl_fin, 0);
	out_jmp();

	/* stack code */
	out_label(lbl_stack);

	/* prepare for joining later */
	out_phi_pop_to(&vphi_buf);

	gen_expr(e->lhs);
	/* va */
	out_change_type(type_ptr_to(type_nav_btype(cc1_type_nav, type_void)));
	out_push_l(
			type_nav_btype(cc1_type_nav, type_long),
			mem_overflow_arg_area->bits.var.struct_offset);

	out_op(op_plus);
	/* &overflow_a */

	/*out_set_lvalue(); * overflow entry in the struct is an lvalue */

	out_dup(), out_change_type(type_ptr_to(type_nav_btype(cc1_type_nav, type_long))), out_deref();
	/* &overflow_a, overflow_a */

	/* XXX: pws will need changing if we jump directly to stack, e.g. passing a struct */
	out_push_l(type_nav_btype(cc1_type_nav, type_long), ws);
	out_op(op_plus);

	out_store();

	out_push_l(type_nav_btype(cc1_type_nav, type_long), ws);
	out_op(op_minus);

	/* ensure we match the other block's final result before the merge */
	out_phi_join(vphi_buf);

	/* "merge" */
	out_label(lbl_fin);

	/* now have a pointer to the right memory address */
	out_change_type(type_ptr_to(ty));
	out_deref();

	/*
	 * this works by using phi magic - we end up with something like this:
	 *
	 *   <reg calc>
	 *   // pointer in rbx
	 *   jmp fin
	 * else:
	 *   <stack calc>
	 *   // pointer in rax
	 *   <phi-merge with previous block>
	 * fin:
	 *   ...
	 *
	 * This is because the two parts of the if above are disjoint, one may
	 * leave its result in eax, one in ebx. We need basic blocks and phi:s to
	 * solve this properly.
	 *
	 * This problem exists in other code, such as &&-gen, but since we pop
	 * and push immediately, it doesn't manifest itself.
	 */

	free(lbl_stack);
	free(lbl_fin);
}

static void builtin_gen_va_arg(expr *e)
{
#ifdef UCC_VA_ABI
	/*
	 * first 4 bytes are offset into saved_regs
	 * second 4 bytes are offset into saved_var_stack
	 *
	 * va_list val;
	 *
	 * if(val[0] + nargs < N_CALL_REGS)
	 *   __builtin_frame_address(0) - pws * (nargs) - (intptr_t)++val[0]
	 * else
	 *   __builtin_frame_address(0) + pws() * (nargs - N_CALL_REGS) + val[1]++
	 *
	 * if becomes:
	 *   if(val[0] < N_CALL_REGS - nargs)
	 * since N_CALL_REGS-nargs can be calculated at compile time
	 */
	/* finally store the number of arguments to this function */
	const int nargs = e->bits.n;
	char *lbl_else = out_label_code("va_arg_overflow"),
			 *lbl_fin  = out_label_code("va_arg_fin");

	out_comment("va_arg start");

	lea_expr(e->lhs, stab);
	/* &va */

	out_dup();
	/* &va, &va */

	out_change_type(type_new_LONG_PTR());
	out_deref();
	/* &va, va */

	/* out_n_call_regs() has been revoked - UCC ABI is obsolete */
	out_push_l(type_new_LONG(), out_n_call_regs() - nargs);
	out_op(op_lt);
	/* &va, (<) */

	out_jfalse(lbl_else);
	/* &va */

	/* __builtin_frame_address(0) - nargs
	 * - multiply by pws is implicit - void *
	 */
	out_push_frame_ptr(0);
	out_change_type(type_new_LONG_PTR());
	out_push_l(type_new_INTPTR_T(), nargs);
	out_op(op_minus);
	/* &va, va_ptr */

	/* - (intptr_t)val[0]++  */
	out_swap(); /* pull &val to the top */

	/* va_ptr, &va */
	out_dup();
	out_change_type(type_new_LONG_PTR());
	/* va_ptr, (long *)&va, (int *)&va */

	out_deref();
	/* va_ptr, &va, va */

	out_push_l(type_new_INTPTR_T(), 1);
	out_op(op_plus); /* val[0]++ */
	/* va_ptr, &va, (va+1) */
	out_store();
	/* va_ptr, (va+1) */

	out_op(op_minus);
	/* va_ptr - (va+1) */
	/* va_ptr - va - 1 = va_ptr_arg-1 */

	EOF_WHERE(&e->where,
		out_change_type(type_new_ptr(e->tree_type, qual_none));
	);
	out_deref();
	/* *va_arg() */

	out_push_lbl(lbl_fin, 0);
	out_jmp();
	out_label(lbl_else);

	out_comment("TODO");
	out_undefined();

	out_label(lbl_fin);

	free(lbl_else);
	free(lbl_fin);

	out_comment("va_arg end");
#elif defined(UCC_ABI_EXTERNAL)

	out_push_lbl("__va_arg", 1);

	/* generate a call to abi.c's __va_arg */
	out_push_l(type_new_LONG(), type_size(e->bits.tref, NULL));
	/* 0 - abi.c's gen_reg. this is temporary until we have builtin_va_arg proper */
	out_push_zero(type_new_INT());
	gen_expr(e->lhs);

	extern void *funcargs_new(); /* XXX: temporary hack for the call */

	out_call(3, type_new_ptr(e->bits.tref, qual_none),
			type_new_func(type_new_VOID(), funcargs_new()));

	out_deref(); /* __va_arg returns a pointer to the stack location of the argument */
#else
	{
		type *const ty = e->bits.va_arg_type;

		if(type_is_s_or_u(ty)){
			ICE("TODO: s/u/e va_arg");
stack:
			ICE("TODO: stack __builtin_va_arg()");

		}else{
			const btype *typ = type_get_type(ty);
			const int fp = typ && type_floating(typ->primitive);
			struct_union_enum_st *sue_va;

			if(typ && typ->primitive == type_ldouble)
				goto stack;

			sue_va = type_next(
						type_nav_va_list(cc1_type_nav, NULL)
					)->bits.type->sue;

#define VA_DECL(nam) \
			decl *mem_ ## nam = struct_union_member_find(sue_va, #nam, NULL, NULL)
			VA_DECL(gp_offset);
			VA_DECL(fp_offset);
			VA_DECL(reg_save_area);
			VA_DECL(overflow_arg_area);

			va_arg_gen_read(
					e,
					ty,
					fp ? mem_fp_offset : mem_gp_offset,
					mem_reg_save_area,
					mem_overflow_arg_area);
		}
	}

#endif
}

static void fold_va_arg(expr *e, symtable *stab)
{
	type *const ty = e->bits.va_arg_type;
	type *to;

	FOLD_EXPR(e->lhs, stab);
	fold_type(ty, stab);

	va_type_check(e->lhs, e->expr, stab);

	if(type_is_promotable(ty, &to)){
		char tbuf[TYPE_STATIC_BUFSIZ];

		warn_at(&e->where,
				"va_arg(..., %s) has undefined behaviour - promote to %s",
				type_to_str(ty), type_to_str_r(tbuf, to));
	}

	e->tree_type = ty;

#ifdef UCC_VA_ABI
	/* finally store the number of arguments to this function */
	e->bits.n = dynarray_count(
			type_funcargs(
				curdecl_func->ref)->arglist)
#endif
}

expr *parse_va_arg(const char *ident, symtable *scope)
{
	/* va_arg(list, type) */
	expr *fcall = expr_new_funcall();
	expr *list = PARSE_EXPR_NO_COMMA(scope);
	type *ty;

	(void)ident;

	EAT(token_comma);
	ty = parse_type(0, scope);

	fcall->lhs = list;
	fcall->bits.va_arg_type = ty;

	expr_mutate_builtin_gen(fcall, va_arg);

	return fcall;
}

static void builtin_gen_va_end(expr *e)
{
	(void)e;
	out_push_noop();
}

static void fold_va_end(expr *e, symtable *stab)
{
	if(dynarray_count(e->funcargs) != 1)
		die_at(&e->where, "%s requires one argument", BUILTIN_SPEL(e->expr));

	FOLD_EXPR(e->funcargs[0], stab);
	va_type_check(e->funcargs[0], e->expr, stab);

	/*va_ensure_variadic(e, stab); - va_end can be anywhere */

	e->tree_type = type_nav_btype(cc1_type_nav, type_void);
}

expr *parse_va_end(const char *ident, symtable *scope)
{
	expr *fcall = parse_any_args(scope);

	(void)ident;
	expr_mutate_builtin_gen(fcall, va_end);
	return fcall;
}

static void builtin_gen_va_copy(expr *e)
{
	gen_expr(e->lhs);
}

static void fold_va_copy(expr *e, symtable *stab)
{
	int i;

	if(dynarray_count(e->funcargs) != 2)
		die_at(&e->where, "%s requires two arguments", BUILTIN_SPEL(e->expr));

	for(i = 0; i < 2; i++){
		FOLD_EXPR(e->funcargs[i], stab);
		va_type_check(e->funcargs[i], e->expr, stab);
	}

	/* (*a) = (*b) */
	e->lhs = builtin_new_memcpy(
			expr_new_deref(e->funcargs[0]),
			expr_new_deref(e->funcargs[1]),
			type_size(type_nav_va_list(cc1_type_nav, stab), &e->where));

	FOLD_EXPR(e->lhs, stab);

	e->tree_type = type_nav_btype(cc1_type_nav, type_void);
}

expr *parse_va_copy(const char *ident, symtable *scope)
{
	expr *fcall = parse_any_args(scope);
	(void)ident;
	expr_mutate_builtin_gen(fcall, va_copy);
	return fcall;
}
