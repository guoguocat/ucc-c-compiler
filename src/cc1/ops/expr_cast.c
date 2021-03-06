#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "ops.h"
#include "../../util/alloc.h"
#include "../../util/platform.h"
#include "expr_cast.h"
#include "../sue.h"
#include "../defs.h"
#include "../type_is.h"
#include "../type_nav.h"

#define IMPLICIT_STR(e) ((e)->expr_cast_implicit ? "implicit " : "")
#define IS_RVAL_CAST(e)  (!(e)->bits.cast.tref)
#define IS_DECAY_CAST(e) ((e)->bits.cast.tref && e->bits.cast.is_decay)

static integral_t convert_integral_to_integral_warn(
		const integral_t in, type *tin,
		type *tout,
		int do_warn, where *w);


const char *str_expr_cast()
{
	return "cast";
}

static void fold_cast_num(expr *const e, numeric *const num)
{
	int to_fp, from_fp;

	to_fp = type_is_floating(e->tree_type);
	from_fp = type_is_floating(expr_cast_child(e)->tree_type);

	if(to_fp){
		if(from_fp){
			UCC_ASSERT(K_FLOATING(*num), "i/f mismatch types");
			/* float -> float - nothing to see here */
		}else{
			UCC_ASSERT(K_INTEGRAL(*num), "i/f mismatch types");
			/* int -> float */
			if(num->suffix & VAL_UNSIGNED){
				num->val.f = num->val.i;
			}else{
				/* force a signed conversion, long long to long double */
				num->val.f = (sintegral_t)num->val.i;
			}
		}

		/* perform the trunc */
		switch(type_primitive(e->tree_type)){
			default:
				ICE("fp expected");

#define TRUNC(cse, ty, bmask) \
			case type_ ## cse: \
				num->val.f = (ty)num->val.f; \
				num->suffix = bmask; \
				break

			TRUNC(float, float, VAL_FLOAT);
			TRUNC(double, double, VAL_DOUBLE);
			TRUNC(ldouble, long double, VAL_LDOUBLE);
#undef TRUNC
		}
		return;
	}else if(from_fp){
		UCC_ASSERT(K_FLOATING(*num), "i/f mismatch types");

		/* special case _Bool */
		if(type_is_primitive(e->tree_type, type__Bool)){
			num->val.i = !!num->val.f;
		}else{
			/* float -> int */
			num->val.i = num->val.f;
		}

		num->suffix = 0;

		/* fall through to int logic */
	}

	UCC_ASSERT(K_INTEGRAL(*num), "fp const?");

#define pv (&num->val.i)
	/* need to cast the val.i down as appropriate */
	if(type_is_primitive(e->tree_type, type__Bool)){
		*pv = !!*pv; /* analagous to out/out.c::out_normalise()'s constant case */

	}else if(!from_fp){
		*pv = convert_integral_to_integral_warn(
				*pv, e->expr->tree_type,
				e->tree_type,
				e->expr_cast_implicit, &e->where);
	}
#undef pv
}

static integral_t convert_integral_to_integral_warn(
		const integral_t in, type *tin,
		type *tout,
		int do_warn, where *w)
{
	/*
	 * C99
	 * 6.3.1.3 Signed and unsigned integers
	 *
	 * When a value with integer type is converted to another integer type
	 * other than _Bool, if the value can be represented by the new type, it
	 * is unchanged.
	 *
	 * Otherwise, if the new type is unsigned, the value is converted by
	 * repeatedly adding or subtracting one more than the maximum value that
	 * can be represented in the new type until the value is in the range of
	 * the new type.
	 *
	 * Otherwise, the new type is signed and the value cannot be represented
	 * in it; either the result is implementation-defined or an
	 * implementation-defined signal is raised.
	 */

	/* representable
	 * if size(out) > size(in)
	 * or if size(out) == size(in)
	 *    and conversion is not unsigned -> signed
	 * or conversion is unsigned -> signed and in < signed-max
	 */

	const unsigned sz_in = type_size(tin, w);
	const int signed_in = type_is_signed(tin);
	const int signed_out = type_is_signed(tout);
	sintegral_t to_iv_sign_ext;
	integral_t to_iv = integral_truncate(in, sz_in, &to_iv_sign_ext);
	integral_t ret;

	/* need to sign extend if signed */
	if(signed_in || signed_out)
		ret = (integral_t)to_iv_sign_ext;
	else
		ret = to_iv;

	if(do_warn){
		if(ret != in){
			warn_at(w,
					"implicit cast changes value from %lld to %lld",
					in, ret);

		}else if(signed_out && !signed_in && (sintegral_t)ret < 0){
			warn_at(w,
					"implicit cast to negative changes value from %llu to %lld",
					in, (sintegral_t)to_iv_sign_ext);

		}else if(signed_out ? (sintegral_t)ret > 0 : 1){
			/* ret > 0 - don't warn for -1 <-- -1L */
			int in_high = integral_high_bit(in, tin);
			int out_high = integral_high_bit(type_max(tout, w), tout);

			if(in_high > out_high){
				warn_at(w,
						"implicit cast truncates value from %lld to %lld",
						in, ret & ((1ULL << (out_high + 1)) - 1));
			}
		}
	}

	return ret;
}

static void fold_const_expr_cast(expr *e, consty *k)
{
	int to_fp;

	if(type_is_void(e->tree_type)){
		k->type = CONST_NO;
		return;
	}

	to_fp = type_is_floating(e->tree_type);

	const_fold(expr_cast_child(e), k);

	if(IS_RVAL_CAST(e))
		return;

	switch(k->type){
		case CONST_NUM:
			fold_cast_num(e, &k->bits.num);
			break;

		case CONST_NO:
			break;

		case CONST_NEED_ADDR:
			if(to_fp){
				k->type = CONST_NO;
				break;
			}
			/* fall */

		case CONST_ADDR:
		case CONST_STRK:
		{
			int l, r;

			if(to_fp){
				/* had an error - reported in fold() */
				k->type = CONST_NO;
				return;
			}

			/* allow if we're casting to a same-size type */
			l = type_size(e->tree_type, &e->where);

			if(type_decayable(expr_cast_child(e)->tree_type))
				r = platform_word_size(); /* func-ptr or array->ptr */
			else
				r = type_size(expr_cast_child(e)->tree_type, &expr_cast_child(e)->where);

			if(l < r){
				/* shouldn't fit, check if it will */
				switch(k->type){
					default:
						ICE("bad switch");

					case CONST_STRK:
						/* no idea where it will be in memory,
						 * can't fit into a smaller type */
						k->type = CONST_NO; /* e.g. (int)&a */
						break;

					case CONST_NEED_ADDR:
					case CONST_ADDR:
						if(k->bits.addr.is_lbl){
							k->type = CONST_NO; /* similar to strk case */
						}else{
							integral_t new = k->bits.addr.bits.memaddr;
							const int pws = platform_word_size();

							/* mask out bits so we have it truncated to `l' */
							if(l < pws){
								new = integral_truncate(new, l, NULL);

								if(k->bits.addr.bits.memaddr != new)
									/* can't cast without losing value - not const */
									k->type = CONST_NO;

							}else{
								/* what are you doing... */
								k->type = CONST_NO;
							}
						}
				}
			}
			break;
		}
	}

	/* if casting from pointer to int, it's not a constant
	 * but we treat it as such, as an extension */
	if(type_is_ptr(e->expr->tree_type)
	&& !type_is_ptr(e->tree_type)
	&& !k->nonstandard_const)
	{
		k->nonstandard_const = e;
	}
}

static void lea_expr_cast(expr *e)
{
	expr *c = expr_cast_child(e);
	c->f_lea(c);
}

void fold_expr_cast_descend(expr *e, symtable *stab, int descend)
{
	int flag;
	type *tlhs, *trhs;

	if(descend)
		FOLD_EXPR(expr_cast_child(e), stab);

	if(IS_RVAL_CAST(e)){
		/* remove cv-qualifiers */
		e->tree_type = type_unqualify(expr_cast_child(e)->tree_type);

		/* rval cast can have a lea */
		if(expr_cast_child(e)->f_lea)
			e->f_lea = lea_expr_cast;

	}else{
		/* casts remove restrict qualifiers */
		enum type_qualifier q = type_qual(e->bits.cast.tref);

		e->tree_type = type_qualify(e->bits.cast.tref, q & ~qual_restrict);

		fold_type(e->tree_type, stab); /* struct lookup, etc */

		tlhs = e->tree_type;
		trhs = expr_cast_child(e)->tree_type;

		if(!IS_DECAY_CAST(e)){
			int size_lhs, size_rhs;

			fold_check_expr(expr_cast_child(e),
					FOLD_CHK_NO_ST_UN | FOLD_CHK_ALLOW_VOID,
					"cast-expr");

			if(type_is_void(tlhs))
				return; /* fine */
			fold_check_expr(e,
					FOLD_CHK_NO_ST_UN | FOLD_CHK_ALLOW_VOID,
					"cast-target");

			if(!type_is_complete(tlhs)){
				die_at(&e->where, "%scast to incomplete type %s",
						IMPLICIT_STR(e),
						type_to_str(tlhs));
			}

			if((flag = !!type_is(tlhs, type_func))
			|| type_is(tlhs, type_array))
			{
				die_at(&e->where, "%scast to %s type '%s'",
						IMPLICIT_STR(e),
						flag ? "function" : "array",
						type_to_str(tlhs));
			}

			if(((flag = !!type_is_ptr(tlhs)) && type_is_floating(trhs))
			||           (type_is_ptr(trhs)  && type_is_floating(tlhs)))
			{
				/* TODO: factor to a error-continuing function */
				fold_had_error = 1;
				warn_at_print_error(&e->where,
						"%scast %s pointer %s floating type",
						IMPLICIT_STR(e),
						flag ? "to" : "from",
						flag ? "from" : "to");
				return;
			}

			size_lhs = type_size(tlhs, &e->where);
			size_rhs = type_size(trhs, &expr_cast_child(e)->where);
			if(size_lhs < size_rhs){
				char buf[DECL_STATIC_BUFSIZ];

				strcpy(buf, type_to_str(trhs));

				cc1_warn_at(&e->where, 0, WARN_LOSS_PRECISION,
						"possible loss of precision %s, size %d <-- %s, size %d",
						type_to_str(tlhs), size_lhs,
						buf, size_rhs);
			}

			if((flag = (type_is_fptr(tlhs) && type_is_nonfptr(trhs)))
			||         (type_is_fptr(trhs) && type_is_nonfptr(tlhs)))
			{
				/* allow cast from NULL to func ptr */
				if(!expr_is_null_ptr(expr_cast_child(e), NULL_STRICT_VOID_PTR)){
					char buf[TYPE_STATIC_BUFSIZ];

					warn_at(&e->where, "%scast from %spointer to %spointer\n"
							"%s <- %s",
							IMPLICIT_STR(e),
							flag ? "" : "function-", flag ? "function-" : "",
							type_to_str(tlhs), type_to_str_r(buf, trhs));
				}
			}

#ifdef W_QUAL
			if(decl_is_ptr(tlhs) && decl_is_ptr(trhs) && (tlhs->type->qual | trhs->type->qual) != tlhs->type->qual){
				const enum type_qualifier away = trhs->type->qual & ~tlhs->type->qual;
				char *buf = type_qual_to_str(away);
				char *p;

				p = &buf[strlen(buf)-1];
				if(p >= buf && *p == ' ')
					*p = '\0';

				warn_at(&e->where, "%scast removes qualifiers (%s)",
						IMPLICIT_STR(e), buf);
			}
#endif
		}
	}
}

void fold_expr_cast(expr *e, symtable *stab)
{
	fold_expr_cast_descend(e, stab, 1);
}

void gen_expr_cast(expr *e)
{
	gen_expr(expr_cast_child(e));

	if(IS_RVAL_CAST(e)){
		/*out_to_rvalue();*/
	}else{
		type *tto, *tfrom;

		tto = e->tree_type;
		tfrom = expr_cast_child(e)->tree_type;

		/* return if cast-to-void */
		if(type_is_void(tto)){
			out_change_type(tto);
			out_comment("cast to void");
			return;
		}

		if(fopt_mode & FOPT_PLAN9_EXTENSIONS){
			/* allow b to be an anonymous member of a */
			struct_union_enum_st *a_sue = type_is_s_or_u(type_is_ptr(tto)),
													 *b_sue = type_is_s_or_u(type_is_ptr(tfrom));

			if(a_sue && b_sue && a_sue != b_sue){
				decl *mem = struct_union_member_find_sue(b_sue, a_sue);

				if(mem){
					/*char buf[TYPE_STATIC_BUFSIZ];
						fprintf(stderr, "CAST %s -> %s, adj by %d\n",
						type_to_str(tfrom),
						type_to_str_r(buf, tto),
						mem->struct_offset);*/

					out_change_type(type_ptr_to(
								type_nav_btype(cc1_type_nav, type_void)));

					out_push_l(type_nav_btype(cc1_type_nav, type_intptr_t),
							mem->bits.var.struct_offset);
					out_op(op_plus);
				}
			}
		}

		out_cast(tto, /*normalise_bool:*/1);
	}
}

void gen_expr_str_cast(expr *e)
{
	idt_printf("%scast expr:\n", IS_RVAL_CAST(e) ? "rvalue-" : "");
	gen_str_indent++;
	print_expr(expr_cast_child(e));
	gen_str_indent--;
}

void mutate_expr_cast(expr *e)
{
	e->f_const_fold = fold_const_expr_cast;
}

expr *expr_new_cast(expr *what, type *to, int implicit)
{
	expr *e = expr_new_wrapper(cast);
	e->bits.cast.tref = to;
	e->expr_cast_implicit = implicit;
	expr_cast_child(e) = what;
	return e;
}

expr *expr_new_cast_rval(expr *sub)
{
	expr *e = expr_new_wrapper(cast);
	/* mark as rvalue cast */
	e->bits.cast.tref = NULL;
	e->bits.cast.is_decay = 0;
	expr_cast_child(e) = sub;
	return e;
}

expr *expr_new_cast_decay(expr *sub, type *to)
{
	expr *e = expr_new_wrapper(cast);
	/* mark as decay */
	e->bits.cast.tref = to;
	e->bits.cast.is_decay = 1;
	expr_cast_child(e) = sub;
	return e;
}

void gen_expr_style_cast(expr *e)
{
	stylef("(%s)", type_to_str(e->bits.cast.tref));
	gen_expr(expr_cast_child(e));
}
