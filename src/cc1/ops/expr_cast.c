#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../../util/alloc.h"
#include "ops.h"
#include "expr_cast.h"
#include "../out/asm.h"

const char *str_expr_cast()
{
	return "cast";
}

void fold_const_expr_cast(expr *e, consty *k)
{
#define piv (&k->bits.iv)
	const_fold(e->expr, k);

	if(k->type == CONST_VAL){
		/* need to cast the val down as appropriate */
		if(type_ref_is_type(e->tree_type, type__Bool)){
			piv->val = !!piv->val; /* analagous to out/out.c::out_normalise()'s constant case */

		}else{
			const int sz = type_ref_size(e->tree_type, &e->where);

			/* TODO: disallow for ptrs/non-ints */

			switch(sz){
				/* TODO: unsigned */

/*
#define CAST(sz, t) case sz: piv->val = (t)piv->val; break  - don't use host machine casting
*/
#define CAST(sz, t) case sz: piv->val = piv->val & ~(-1 << (sz * 8 - 1)); break

				CAST(1, char);
				CAST(2, short);
				CAST(4, int);

#undef CAST

				case 8:
				break; /* no cast - max word size */

				default:
				k->type = CONST_NO;
				ICW("can't const fold cast expr of type %s size %d",
						type_ref_to_str(e->tree_type), sz);
			}
		}
	}
#undef piv
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
		DIE_AT(&e->where, "cast to incomplete type %s", type_ref_to_str(e->tree_type));

	if((flag = !!type_ref_is(e->tree_type, type_ref_func)) || type_ref_is(e->tree_type, type_ref_array))
		DIE_AT(&e->where, "cast to %s type '%s'", flag ? "function" : "array", type_ref_to_str(e->tree_type));

#ifdef CAST_COLLAPSE
	if(expr_kind(e->expr, cast)){
		/* get rid of e->expr, replace with e->expr->rhs */
		expr *del = e->expr;

		e->expr = e->expr->expr;

		/*decl_free(del->tree_type); XXX: memleak */
		expr_free(del);

		fold_expr_cast(e, stab);
	}
#endif

	tlhs = e->tree_type;
	trhs = e->expr->tree_type;

	if(!type_ref_is_void(tlhs) && (size_lhs = asm_type_size(tlhs)) < (size_rhs = asm_type_size(trhs))){
		char buf[DECL_STATIC_BUFSIZ];

		strcpy(buf, type_ref_to_str(trhs));

		cc1_warn_at(&e->where, 0, 1, WARN_LOSS_PRECISION,
				"possible loss of precision %s, size %d <-- %s, size %d",
				type_ref_to_str(tlhs), size_lhs,
				buf, size_rhs);
	}

#ifdef W_QUAL
	if(decl_is_ptr(tlhs) && decl_is_ptr(trhs) && (tlhs->type->qual | trhs->type->qual) != tlhs->type->qual){
		const enum type_qualifier away = trhs->type->qual & ~tlhs->type->qual;
		char *buf = type_qual_to_str(away);
		char *p;

		p = &buf[strlen(buf)-1];
		if(p >= buf && *p == ' ')
			*p = '\0';

		WARN_AT(&e->where, "casting away qualifiers (%s)", buf);
	}
#endif
}

void fold_expr_cast(expr *e, symtable *stab)
{
	fold_expr_cast_descend(e, stab, 1);
}

void gen_expr_cast(expr *e, symtable *stab)
{
	type_ref *tto, *tfrom;

	gen_expr(e->expr, stab);

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

	out_cast(tfrom, tto);

	if(type_ref_is_type(tto, type__Bool)) /* 1 or 0 */
		out_normalise();
}

void gen_expr_str_cast(expr *e, symtable *stab)
{
	(void)stab;
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

void gen_expr_style_cast(expr *e, symtable *stab)
{ (void)e; (void)stab; /* TODO */ }
