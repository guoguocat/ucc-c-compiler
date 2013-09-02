#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "ops.h"
#include "../../util/alloc.h"
#include "../../util/platform.h"
#include "expr_cast.h"
#include "../out/asm.h"
#include "../sue.h"
#include "../defs.h"

const char *str_expr_cast()
{
	return "cast";
}

static void get_cast_sizes(type_ref *tlhs, type_ref *trhs, int *pl, int *pr)
{
	if(!type_ref_is_void(tlhs)){
		*pl = asm_type_size(tlhs);
		*pr = asm_type_size(trhs);
	}else{
		*pl = *pr = 0;
	}
}

static void fold_const_expr_cast(expr *e, consty *k)
{
	if(type_ref_is_void(e->tree_type)){
		k->type = CONST_NO;
		return;
	}

	const_fold(e->expr, k);

	switch(k->type){
		case CONST_VAL:
#define piv (&k->bits.iv)
			/* need to cast the val down as appropriate */
			if(type_ref_is_type(e->tree_type, type__Bool)){
				piv->val = !!piv->val; /* analagous to out/out.c::out_normalise()'s constant case */

			}else if(e->expr_cast_implicit){ /* otherwise this is a no-op */
				const unsigned sz = type_ref_size(e->tree_type, &e->where);
				const intval_t old = piv->val;
				const int to_sig   = type_ref_is_signed(e->tree_type);
				const int from_sig = type_ref_is_signed(e->expr->tree_type);
				intval_t to_iv, to_iv_sign_ext;

				/* TODO: disallow for ptrs/non-ints */

				/* we don't save the truncated value - we keep the original
				 * so negative numbers, for example, are preserved */
				to_iv = intval_truncate(piv->val, sz, &to_iv_sign_ext);

				if(to_sig && from_sig ? old != to_iv_sign_ext : old != to_iv){
#define CAST_WARN(pre_fmt, pre_val, post_fmt, post_val)  \
						warn_at(&e->where,                           \
								"implicit cast changes value from %"     \
								pre_fmt " to %" post_fmt,                \
								pre_val, post_val)

					/* nice... */
					if(from_sig){
						if(to_sig)
							CAST_WARN(
									INTVAL_FMT_D, (long long signed)old,
									INTVAL_FMT_D, (long long signed)to_iv_sign_ext);
						else
							CAST_WARN(
									INTVAL_FMT_D, (long long signed)old,
									INTVAL_FMT_U, (long long unsigned)to_iv);
					}else{
						if(to_sig)
							CAST_WARN(
									INTVAL_FMT_U, (long long unsigned)old,
									INTVAL_FMT_D, (long long signed)to_iv_sign_ext);
						else
							CAST_WARN(
									INTVAL_FMT_U, (long long unsigned)old,
									INTVAL_FMT_U, (long long unsigned)to_iv);
					}
				}
			}
#undef piv
			break;

		case CONST_NO:
			break;

		case CONST_NEED_ADDR: /* allow if we're casting to a same-sized type */
		case CONST_ADDR:
		case CONST_STRK:
		{
			int l, r;
			/* allow if we're casting to a same-size type */
			get_cast_sizes(e->tree_type, e->expr->tree_type, &l, &r);

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
							intval_t new = k->bits.addr.bits.memaddr;
							const int pws = platform_word_size();

							/* mask out bits so we have it truncated to `l' */
							if(l < pws){
								new = intval_truncate(new, l, NULL);

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
}

void fold_expr_cast_descend(expr *e, symtable *stab, int descend)
{
	int size_lhs, size_rhs;
	int flag;
	type_ref *tlhs, *trhs;

	if(descend)
		FOLD_EXPR(e->expr, stab);

	/* casts remove restrict qualifiers */
	{
		enum type_qualifier q = type_ref_qual(e->bits.tref);

		e->tree_type = type_ref_new_cast(e->bits.tref, q & ~qual_restrict);
	}

	fold_type_ref(e->tree_type, NULL, stab); /* struct lookup, etc */

	fold_disallow_st_un(e->expr, "cast-expr");
	fold_disallow_st_un(e, "cast-target");

	if(!type_ref_is_complete(e->tree_type) && !type_ref_is_void(e->tree_type))
		die_at(&e->where, "cast to incomplete type %s", type_ref_to_str(e->tree_type));

	if((flag = !!type_ref_is(e->tree_type, type_ref_func)) || type_ref_is(e->tree_type, type_ref_array))
		die_at(&e->where, "cast to %s type '%s'", flag ? "function" : "array", type_ref_to_str(e->tree_type));

	tlhs = e->tree_type;
	trhs = e->expr->tree_type;

	get_cast_sizes(tlhs, trhs, &size_lhs, &size_rhs);
	if(size_lhs < size_rhs){
		char buf[DECL_STATIC_BUFSIZ];

		strcpy(buf, type_ref_to_str(trhs));

		cc1_warn_at(&e->where, 0, WARN_LOSS_PRECISION,
				"possible loss of precision %s, size %d <-- %s, size %d",
				type_ref_to_str(tlhs), size_lhs,
				buf, size_rhs);
	}

	if((flag = (type_ref_is_fptr(tlhs) && type_ref_is_nonfptr(trhs)))
	||         (type_ref_is_fptr(trhs) && type_ref_is_nonfptr(tlhs)))
	{
		char buf[TYPE_REF_STATIC_BUFSIZ];
		warn_at(&e->where, "%scast from %spointer to %spointer\n"
				"%s <- %s",
				e->expr_cast_implicit ? "implicit " : "",
				flag ? "" : "function-", flag ? "function-" : "",
				type_ref_to_str(tlhs), type_ref_to_str_r(buf, trhs));
	}

#ifdef W_QUAL
	if(decl_is_ptr(tlhs) && decl_is_ptr(trhs) && (tlhs->type->qual | trhs->type->qual) != tlhs->type->qual){
		const enum type_qualifier away = trhs->type->qual & ~tlhs->type->qual;
		char *buf = type_qual_to_str(away);
		char *p;

		p = &buf[strlen(buf)-1];
		if(p >= buf && *p == ' ')
			*p = '\0';

		warn_at(&e->where, "casting away qualifiers (%s)", buf);
	}
#endif
}

void fold_expr_cast(expr *e, symtable *stab)
{
	fold_expr_cast_descend(e, stab, 1);
}

void gen_expr_cast(expr *e)
{
	type_ref *tto, *tfrom;

	gen_expr(e->expr);

	tto = e->tree_type;
	tfrom = e->expr->tree_type;

	/* return if cast-to-void */
	if(type_ref_is_void(tto)){
		out_change_type(tto);
		out_comment("cast to void");
		return;
	}

	/* check float <--> int conversion */
	if(type_ref_is_floating(tto) != type_ref_is_floating(tfrom))
		ICE("TODO: float <-> int casting");

	if(fopt_mode & FOPT_PLAN9_EXTENSIONS){
		/* allow b to be an anonymous member of a */
		struct_union_enum_st *a_sue = type_ref_is_s_or_u(type_ref_is_ptr(tto)),
												 *b_sue = type_ref_is_s_or_u(type_ref_is_ptr(tfrom));

		if(a_sue && b_sue && a_sue != b_sue){
			decl *mem = struct_union_member_find_sue(b_sue, a_sue);

			if(mem){
				/*char buf[TYPE_REF_STATIC_BUFSIZ];
				fprintf(stderr, "CAST %s -> %s, adj by %d\n",
						type_ref_to_str(tfrom),
						type_ref_to_str_r(buf, tto),
						mem->struct_offset);*/

				out_change_type(type_ref_cached_VOID_PTR());
				out_push_i(type_ref_cached_INTPTR_T(), mem->struct_offset);
				out_op(op_plus);
			}
		}
	}

	out_cast(tto);

	if(type_ref_is_type(tto, type__Bool)) /* 1 or 0 */
		out_normalise();
}

void gen_expr_str_cast(expr *e)
{
	idt_printf("cast expr:\n");
	gen_str_indent++;
	print_expr(e->expr);
	gen_str_indent--;
}

void mutate_expr_cast(expr *e)
{
	e->f_const_fold = fold_const_expr_cast;
}

expr *expr_new_cast(type_ref *to, int implicit)
{
	expr *e = expr_new_wrapper(cast);
	e->bits.tref = to;
	e->expr_cast_implicit = implicit;
	return e;
}

void gen_expr_style_cast(expr *e)
{
	stylef("(%s)", type_ref_to_str(e->bits.tref));
	gen_expr(e->expr);
}
