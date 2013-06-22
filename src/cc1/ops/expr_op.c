#include <string.h>
#include <stdlib.h>

#include "../defs.h"
#include "ops.h"
#include "expr_op.h"
#include "../out/lbl.h"
#include "../out/asm.h"

const char *str_expr_op()
{
	return "op";
}

static intval_t operate(
		intval_t lval, intval_t *rval, /* rval is optional */
		enum op_type op, int is_signed,
		const char **error)
{
	typedef sintval_t S;
	typedef  intval_t U;

	/* FIXME: casts based on lval.type */
#define piv (&konst->bits.iv)

#define S_OP(o) (S)lval o (S)*rval
#define U_OP(o) (U)lval o (U)*rval

#define OP(  a, b) case a: return S_OP(b)
#define OP_U(a, b) case a: return is_signed ? S_OP(b) : U_OP(b)

	switch(op){
		OP(op_multiply,   *);
		OP(op_eq,         ==);
		OP(op_ne,         !=);

		OP_U(op_le,       <=);
		OP_U(op_lt,       <);
		OP_U(op_ge,       >=);
		OP_U(op_gt,       >);

		OP(op_xor,        ^);
		OP(op_or,         |);
		OP(op_and,        &);
		OP(op_orsc,       ||);
		OP(op_andsc,      &&);
		OP(op_shiftl,     <<);
		OP(op_shiftr,     >>);

		case op_modulus:
		case op_divide:
			if(*rval)
				return op == op_divide ? lval / *rval : lval % *rval;

			*error = "division by zero";
			return 0;

		case op_plus:
			return lval + (rval ? *rval : 0);

		case op_minus:
			return rval ? lval - *rval : -lval;

		case op_not:  return !lval;
		case op_bnot: return ~lval;

		case op_unknown:
			break;
	}

	ICE("unhandled type");
#undef piv
}

static void const_offset(consty *r, consty *val, consty *addr,
		type_ref *addr_type, enum op_type op)
{
	unsigned step = type_ref_size(type_ref_next(addr_type), NULL);
	int change;

	memcpy_safe(r, addr);

	change = val->bits.iv.val * step;

	if(op == op_minus)
		change = -change;

	/* may already have an offset, hence += */
	r->offset += change;
}

void fold_const_expr_op(expr *e, consty *k)
{
	consty lhs, rhs;

	memset(k, 0, sizeof *k);

	const_fold(e->lhs, &lhs);
	if(e->rhs){
		const_fold(e->rhs, &rhs);

		if(rhs.type == CONST_VAL){
			switch(e->op){
				case op_shiftl:
				case op_shiftr:
				{
					const unsigned ty_sz = CHAR_BIT * type_ref_size(e->lhs->tree_type, &e->lhs->where);
					int undefined = 0;

					if(type_ref_is_signed(e->rhs->tree_type)
					&& (sintval_t)rhs.bits.iv.val < 0)
					{
						WARN_AT(&e->rhs->where, "shift count is negative (%"
								INTVAL_FMT_D ")", (sintval_t)rhs.bits.iv.val);

						undefined = 1;
					}else if(rhs.bits.iv.val >= ty_sz){
						WARN_AT(&e->rhs->where, "shift count >= width of %s (%u)",
								type_ref_to_str(e->lhs->tree_type), ty_sz);

						undefined = 1;
					}


					if(undefined){
						if(lhs.type == CONST_VAL){
							/* already 0 */
							k->type = CONST_VAL;
						}else{
							k->type = CONST_NO;
						}
						return;
					}
				}
				default:
					break;
			}
		}
	}else{
		memset(&rhs, 0, sizeof rhs);
		rhs.type = CONST_VAL;
	}

	if(lhs.type == CONST_VAL && rhs.type == CONST_VAL){
		const char *err = NULL;
		intval_t r;
		/* the op is signed if an operand is, not the result,
		 * e.g. u_a < u_b produces a bool (signed) */
		int is_signed = type_ref_is_signed(e->lhs->tree_type) ||
		                (e->rhs ? type_ref_is_signed(e->rhs->tree_type) : 0);

		r = operate(
				lhs.bits.iv.val,
				e->rhs ? &rhs.bits.iv.val : NULL,
				e->op, is_signed, &err);

		if(err){
			WARN_AT(&e->where, "%s", err);
		}else{
			k->type = CONST_VAL;
			k->bits.iv.val = r;
		}

	}else if((e->op == op_andsc || e->op == op_orsc)
	&& (CONST_AT_COMPILE_TIME(lhs.type) || CONST_AT_COMPILE_TIME(rhs.type))){

		/* allow 1 || f() */
		consty *kside = CONST_AT_COMPILE_TIME(lhs.type) ? &lhs : &rhs;
		int is_true = !!kside->bits.iv.val;

		/* TODO: to be more conformant we should disallow: a() && 0
		 * i.e. ordering
		 */

		if(e->op == (is_true ? op_orsc : op_andsc))
			memcpy(k, kside, sizeof *k);

	}else if(e->op == op_plus || e->op == op_minus){
		/* allow one CONST_{ADDR,STRK} and one CONST_VAL for an offset const */
		int lhs_addr = lhs.type == CONST_ADDR || lhs.type == CONST_STRK;
		int rhs_addr = rhs.type == CONST_ADDR || rhs.type == CONST_STRK;

		/**/if(lhs_addr && rhs.type == CONST_VAL)
			const_offset(k, &rhs, &lhs, e->lhs->tree_type, e->op);
		else if(rhs_addr && lhs.type == CONST_VAL)
			const_offset(k, &lhs, &rhs, e->rhs->tree_type, e->op);
	}
}

void expr_promote_int_if_smaller(expr **pe, symtable *stab)
{
	static unsigned sz_int;
	expr *e = *pe;

	if(!sz_int)
		sz_int = type_primitive_size(type_int);

	if(type_ref_size(e->tree_type, &e->where) < sz_int){
		expr *cast;

		UCC_ASSERT(!type_ref_is(e->tree_type, type_ref_ptr),
				"invalid promotion for pointer");

		/* if(type_primitive_size(e->tree_type->type->primitive) >= type_primitive_size(to))
		 *   return;
		 *
		 * insert down-casts too - the tree_type of the expression is still important
		 */

		cast = expr_new_cast(type_ref_new_type(type_new_primitive(type_int)), 1);

		cast->expr = e;

		fold_expr_cast_descend(cast, stab, 0);

		*pe = cast;
	}
}

type_ref *op_required_promotion(
		enum op_type op,
		expr *lhs, expr *rhs,
		where *w,
		type_ref **plhs, type_ref **prhs)
{
	type_ref *resolved = NULL;
	type_ref *const tlhs = lhs->tree_type, *const trhs = rhs->tree_type;

	*plhs = *prhs = NULL;

#if 0
	If either operand is a pointer:
		relation: both must be pointers

	if both operands are pointers:
		must be '-'

	exactly one pointer:
		must be + or -
#endif

	if(type_ref_is_void(tlhs) || type_ref_is_void(trhs))
		DIE_AT(w, "use of void expression");

	{
		const int l_ptr = !!type_ref_is(tlhs, type_ref_ptr);
		const int r_ptr = !!type_ref_is(trhs, type_ref_ptr);

		if(l_ptr && r_ptr){
			char buf[TYPE_REF_STATIC_BUFSIZ];

			if(op == op_minus){
				/* don't allow void * */
				if(!type_ref_equal(tlhs, trhs, DECL_CMP_EXACT_MATCH)){
					DIE_AT(w, "subtraction of distinct pointer types %s and %s",
							type_ref_to_str(tlhs), type_ref_to_str_r(buf, trhs));
				}

				resolved = type_ref_cached_INTPTR_T();

			}else if(op_is_relational(op)){
ptr_relation:
				if(op_is_comparison(op)){
					if(!fold_type_ref_equal(tlhs, trhs, w,
							WARN_COMPARE_MISMATCH, 0,
							l_ptr && r_ptr
							? "comparison of distinct pointer types lacks a cast (%s vs %s)"
							: "comparison between pointer and integer (%s vs %s)",
							type_ref_to_str(tlhs), type_ref_to_str_r(buf, trhs)))
					{
						/* not equal - ptr vs int */
						*(l_ptr ? prhs : plhs) = type_ref_cached_INTPTR_T();
					}
				}

				resolved = type_ref_cached_INT();

			}else{
				DIE_AT(w, "operation between two pointers must be relational or subtraction");
			}

			goto fin;

		}else if(l_ptr || r_ptr){
			/* + or - */

			/* cmp between pointer and integer - missing cast */
			if(op_is_relational(op))
				goto ptr_relation;

			switch(op){
				default:
					DIE_AT(w, "operation between pointer and integer must be + or -");

				case op_minus:
					if(!l_ptr)
						DIE_AT(w, "subtraction of pointer from integer");

				case op_plus:
					break;
			}

			resolved = l_ptr ? tlhs : trhs;

			/* FIXME: promote to unsigned */
			*(l_ptr ? prhs : plhs) = type_ref_cached_INTPTR_T();

			/* + or -, check if we can */
			{
				type_ref *const next = type_ref_next(resolved);

				if(!type_ref_is_complete(next)){
					if(type_ref_is_void(next)){
						WARN_AT(w, "arithmetic on void pointer");
					}else{
						DIE_AT(w, "arithmetic on pointer to incomplete type %s",
								type_ref_to_str(next));
					}
					/* TODO: note: type declared at resolved->where */
				}
			}


			if(type_ref_is_void_ptr(resolved))
				WARN_AT(w, "arithmetic on void pointer");

			goto fin;
		}
	}

#if 0
	If both operands have the same type, then no further conversion is needed.

	Otherwise, if both operands have signed integer types or both have unsigned
	integer types, the operand with the type of lesser integer conversion rank
	is converted to the type of the operand with greater rank.

	Otherwise, if the operand that has unsigned integer type has rank greater
	or equal to the rank of the type of the other operand, then the operand
	with signed integer type is converted to the type of the operand with
	unsigned integer type.

	Otherwise, if the type of the operand with signed integer type can
	represent all of the values of the type of the operand with unsigned
	integer type, then the operand with unsigned integer type is converted to
	the type of the operand with signed integer type.

	Otherwise, both operands are converted to the unsigned integer type
	corresponding to the type of the operand with signed integer type.
#endif

	{
		type_ref *tlarger = NULL;

		if(op == op_shiftl || op == op_shiftr){
			/* fine with any parameter sizes
			 * don't need to match. resolves to lhs,
			 * or int if lhs is smaller (done before this function)
			 */

			UCC_ASSERT(
					type_ref_size(tlhs, &lhs->where)
						>= type_primitive_size(type_int),
					"shift operand should have been promoted");

			resolved = tlhs;

		}else if(op == op_andsc || op == op_orsc){
			/* no promotion */
			resolved = type_ref_cached_BOOL();

		}else{
			const int l_unsigned = !type_ref_is_signed(tlhs),
			          r_unsigned = !type_ref_is_signed(trhs);

			const int l_sz = type_ref_size(tlhs, &lhs->where),
			          r_sz = type_ref_size(trhs, &rhs->where);

			if(l_unsigned == r_unsigned){
				if(l_sz != r_sz){
					const int l_larger = l_sz > r_sz;
					char bufa[TYPE_REF_STATIC_BUFSIZ], bufb[TYPE_REF_STATIC_BUFSIZ];

					/* TODO: needed? */
					fold_type_ref_equal(tlhs, trhs,
							w, WARN_COMPARE_MISMATCH, 0,
							"mismatching types in %s (%s and %s)",
							op_to_str(op),
							type_ref_to_str_r(bufa, tlhs),
							type_ref_to_str_r(bufb, trhs));

					*(l_larger ? prhs : plhs) = (l_larger ? tlhs : trhs);

					tlarger = l_larger ? tlhs : trhs;

				}else{
					/* default to either */
					tlarger = tlhs;
				}

			}else if(l_unsigned ? l_sz >= r_sz : r_sz >= l_sz){
				if(l_unsigned)
					tlarger = *prhs = tlhs;
				else
					tlarger = *plhs = trhs;

			}else if(l_unsigned ? r_sz > l_sz : l_sz > r_sz){
				/* can the signed type represent all of the unsigned type's values?
				 * this is true if signed_type > unsigned_type
				 * - convert unsigned to signed type */

				if(l_unsigned)
					tlarger = *plhs = trhs;
				else
					tlarger = *prhs = tlhs;

			}else{
				/* else convert both to (unsigned)signed_type */
				type_ref *signed_t = l_unsigned ? trhs : tlhs;

				tlarger = *plhs = *prhs = type_ref_new_cast_signed(signed_t, 0);
			}

			/* if we have a _comparison_ (e.g. between enums), convert to int */
			resolved = op_is_relational(op)
				? type_ref_cached_INT()
				: tlarger;
		}
	}

fin:
	UCC_ASSERT(resolved, "no decl from type promotion");

	return resolved; /* XXX: memleak in some cases */
}

type_ref *op_promote_types(
		enum op_type op,
		const char *desc,
		expr **plhs, expr **prhs,
		where *w, symtable *stab)
{
	type_ref *tlhs, *trhs;
	type_ref *resolved;

	resolved = op_required_promotion(op, *plhs, *prhs, w, &tlhs, &trhs);

	if(tlhs)
		fold_insert_casts(tlhs, plhs, stab, w, desc);

	if(trhs)
		fold_insert_casts(trhs, prhs, stab, w, desc);

	return resolved;
}

static expr *expr_is_array_cast(expr *e)
{
	expr *array = e;

	if(expr_kind(array, cast))
		array = array->expr;

	if(type_ref_is(array->tree_type, type_ref_array))
		return array;

	return NULL;
}

void fold_check_bounds(expr *e, int chk_one_past_end)
{
	/* this could be in expr_deref, but it catches more in expr_op */
	expr *array;
	int lhs = 0;

	/* check bounds */
	if(e->op != op_plus && e->op != op_minus)
		return;

	array = expr_is_array_cast(e->lhs);
	if(array)
		lhs = 1;
	else
		array = expr_is_array_cast(e->rhs);

	if(array && !type_ref_is_incomplete_array(array->tree_type)){
		consty k;

		const_fold(lhs ? e->rhs : e->lhs, &k);

		if(k.type == CONST_VAL){
			const size_t sz = type_ref_array_len(array->tree_type);

#define idx k.bits.iv
			if(e->op == op_minus)
				idx.val = -idx.val;

			/* index is allowed to be one past the end, i.e. idx.val == sz */
			if((sintval_t)idx.val < 0
			|| (chk_one_past_end ? idx.val > sz : idx.val == sz))
			{
				/* XXX: note */
				char buf[WHERE_BUF_SIZ];

				WARN_AT(&e->where,
						"index %" INTVAL_FMT_D " out of bounds of array, size %ld\n"
						"%s: note: array declared here",
						idx.val, (long)sz, where_str_r(buf, &array->tree_type->where));
			}
#undef idx
		}
	}
}

static void op_unsigned_cmp_check(expr *e)
{
	switch(e->op){
			int lhs;
		/*case op_gt:*/
		case op_ge:
		case op_lt:
		case op_le:
			if((lhs = !type_ref_is_signed(e->lhs->tree_type))
			||        !type_ref_is_signed(e->rhs->tree_type))
			{
				consty k;

				const_fold(lhs ? e->rhs : e->lhs, &k);

				if(k.type == CONST_VAL){
					const int v = k.bits.iv.val;

					if(v <= 0){
						WARN_AT(&e->where,
								"comparison of unsigned expression %s %d is always %s",
								op_to_str(e->op), v,
								e->op == op_lt || e->op == op_le ? "false" : "true");
					}
				}
			}

		default:
			break;
	}
}

static void msg_if_precedence(expr *sub, where *w,
		enum op_type binary, int (*test)(enum op_type))
{
	if(expr_kind(sub, op)
	&& !sub->in_parens
	&& sub->op != binary
	&& (*test)(sub->op))
	{
		/* ==, !=, <, ... */
		WARN_AT(w, "%s has higher precedence than %s",
				op_to_str(sub->op), op_to_str(binary));
	}
}

static void op_check_precedence(expr *e)
{
	switch(e->op){
		case op_or:
		case op_and:
			msg_if_precedence(e->lhs, &e->where, e->op, op_is_comparison);
			msg_if_precedence(e->rhs, &e->where, e->op, op_is_comparison);
			break;

		case op_andsc:
		case op_orsc:
			msg_if_precedence(e->lhs, &e->where, e->op, op_is_shortcircuit);
			msg_if_precedence(e->rhs, &e->where, e->op, op_is_shortcircuit);
			break;

		default:
			break;
	}
}

void fold_expr_op(expr *e, symtable *stab)
{
	UCC_ASSERT(e->op != op_unknown, "unknown op in expression at %s",
			where_str(&e->where));

	FOLD_EXPR(e->lhs, stab);
	fold_disallow_st_un(e->lhs, op_to_str(e->op));

	if(e->rhs){
		FOLD_EXPR(e->rhs, stab);
		fold_disallow_st_un(e->rhs, op_to_str(e->op));

		expr_promote_int_if_smaller(&e->lhs, stab);
		expr_promote_int_if_smaller(&e->rhs, stab);

		e->tree_type = op_promote_types(e->op, op_to_str(e->op),
				&e->lhs, &e->rhs, &e->where, stab);

		fold_check_bounds(e, 1);
		op_check_precedence(e);
		op_unsigned_cmp_check(e);

	}else{
		/* (except unary-not) can only have operations on integers,
		 * promote to signed int
		 */

		if(e->op == op_not){
			e->tree_type = type_ref_cached_INT();

		}else{
			/* op_bnot */
			type_ref *t_unary = e->lhs->tree_type;

			if(!type_ref_is_integral(t_unary))
				DIE_AT(&e->where, "unary %s applied to type '%s'",
						op_to_str(e->op), type_ref_to_str(t_unary));

			expr_promote_int_if_smaller(&e->lhs, stab);

			e->tree_type = e->lhs->tree_type;
		}
	}
}

void gen_expr_str_op(expr *e)
{
	idt_printf("op: %s\n", op_to_str(e->op));
	gen_str_indent++;

#define PRINT_IF(hs) if(e->hs) print_expr(e->hs)
	PRINT_IF(lhs);
	PRINT_IF(rhs);
#undef PRINT_IF

	gen_str_indent--;
}

static void op_shortcircuit(expr *e)
{
	char *bail = out_label_code("shortcircuit_bail");

	gen_expr(e->lhs);

	out_dup();
	(e->op == op_andsc ? out_jfalse : out_jtrue)(bail);
	out_pop();

	gen_expr(e->rhs);

	out_label(bail);
	free(bail);

	out_normalise();
}

void gen_expr_op(expr *e)
{
	switch(e->op){
		case op_orsc:
		case op_andsc:
			op_shortcircuit(e);
			break;

		case op_unknown:
			ICE("asm_operate: unknown operator got through");

		default:
			gen_expr(e->lhs);

			if(e->rhs){
				gen_expr(e->rhs);

				out_op(e->op);
				out_change_type(e->tree_type);

				if(fopt_mode & FOPT_TRAPV
				&& type_ref_is_integral(e->tree_type)
				&& type_ref_is_signed(e->tree_type))
				{
					char *skip = out_label_code("trapv");
					out_push_overflow();
					out_jfalse(skip);
					out_undefined();
					out_label(skip);
					free(skip);
				}
				/* make sure we get the pointer, for example 2+(int *)p
				 * or the int, e.g. (int *)a && (int *)b -> int */
			}else{
				out_op_unary(e->op);
			}
	}
}

void mutate_expr_op(expr *e)
{
	e->f_const_fold = fold_const_expr_op;
}

expr *expr_new_op(enum op_type op)
{
	expr *e = expr_new_wrapper(op);
	e->op = op;
	return e;
}

expr *expr_new_op2(enum op_type o, expr *l, expr *r)
{
	expr *e = expr_new_op(o);
	e->lhs = l, e->rhs = r;
	return e;
}

void gen_expr_style_op(expr *e)
{
	if(e->rhs){
		gen_expr(e->lhs);
		stylef(" %s ", op_to_str(e->op));
		gen_expr(e->rhs);
	}else{
		stylef("%s ", op_to_str(e->op));
		gen_expr(e->lhs);
	}
}
