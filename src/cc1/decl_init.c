#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>

#include "../util/util.h"
#include "../util/dynarray.h"
#include "../util/alloc.h"
#include "data_structs.h"
#include "cc1.h"
#include "fold.h"
#include "const.h"
#include "macros.h"
#include "sue.h"

#include "decl_init.h"

#define INIT_DEBUG(...) /*fprintf(stderr, __VA_ARGS__)*/

static void decl_init_create_assignments_discard(
		decl_init ***init_iter,
		type_ref *const tfor_wrapped,
		expr *base,
		stmt *init_code);

int decl_init_len(decl_init *di)
{
 switch(di->type){
	 case decl_init_scalar:
		 return 1;

	 case decl_init_brace:
		 return dynarray_count((void **)di->bits.inits);
 }
 ICE("decl init bad type");
 return -1;
}

int decl_init_is_const(decl_init *dinit, symtable *stab)
{
	switch(dinit->type){
		case decl_init_scalar:
		{
			expr *e = dinit->bits.expr;
			intval iv;
			enum constyness type;

			FOLD_EXPR(e, stab);
			const_fold(e, &iv, &type);

			dinit->bits.expr = e;

			return type != CONST_NO;
		}

		case decl_init_brace:
		{
			decl_init **i;

			for(i = dinit->bits.inits; i && *i; i++)
				if(!decl_init_is_const(*i, stab))
					return 0;

			return 1;
		}
	}

	ICE("bad decl init");
	return -1;
}

decl_init *decl_init_new(enum decl_init_type t)
{
	decl_init *di = umalloc(sizeof *di);
	where_new(&di->where);
	di->type = t;
	return di;
}

void decl_init_free_1(decl_init *di)
{
	free(di);
}

const char *decl_init_to_str(enum decl_init_type t)
{
	switch(t){
		CASE_STR_PREFIX(decl_init, scalar);
		CASE_STR_PREFIX(decl_init, brace);
	}
	return NULL;
}

static type_ref *decl_initialise_array(
		decl_init ***init_iter,
		type_ref *const tfor_wrapped, expr *base, stmt *init_code)
{
	decl_init *dinit = **init_iter;
	type_ref *tfor_deref, *tfor;
	int complete_to = 0;

	switch(dinit ? dinit->type : decl_init_brace){
		case decl_init_scalar:
			/* if we're nested, pull as many as we need from the init */
			/* FIXME: can't check tree_type - fold hasn't occured yet */
			if((tfor = type_ref_is(dinit->bits.expr->tree_type, type_ref_ptr))
					&& type_ref_is_type(tfor->ref, type_char))
			{
				/* const char * init - need to check tfor is of the same type */
				ICE("TODO: array init with string literal");
			}

			/* check for scalar init isn't done here */
			break;

		case decl_init_brace:
			break;
	}

	tfor = tfor_wrapped;
	tfor_deref = type_ref_is(tfor_wrapped, type_ref_array)->ref;

	/* walk through the inits, pulling as many as we need/one sub-brace for a sub-init */
	/* e.g.
	 * int x[]    = { 1, 2, 3 };    subinit = int,    pull 1 for each
	 * int x[][2] = { 1, 2, 3, 4 }  subinit = int[2], pull 2 for each
	 *
	 * int x[][2] = { {1}, 2, 3, {4}, 5, {6}, {7} };
	 * subinit = int[2], pull as show:
	 *              { {1}, 2, 3, {4}, 5, {6}, {7} };
	 *                 ^   ^--^   ^   ^   ^    ^      -> 6 inits
	 */

	if(dinit){
		decl_init **array_iter = (dinit->type == decl_init_scalar ? *init_iter : dinit->bits.inits);
		int i;

		for(i = 0; *array_iter; i++){
			/* index into the main-array */
			expr *this;
			{ /* `base`[i] */
				expr *op = expr_new_op(op_plus);
				op->lhs = base;
				op->rhs = expr_new_val(i);
				this = expr_new_deref(op);
			}

			INIT_DEBUG("initalising (%s)[%d] with %s\n",
					type_ref_to_str(tfor), i,
					decl_init_to_str((*array_iter)->type));

			decl_init_create_assignments_discard(
					&array_iter, tfor_deref, this, init_code);
		}

		complete_to = i; /* array_iter - start */
		*init_iter += complete_to;
	}

	/* patch the type size */
	if(type_ref_is_incomplete_array(tfor)){
		tfor = type_ref_complete_array(tfor, complete_to);

		INIT_DEBUG("completed array to %d - %s\n",
				complete_to, type_ref_to_str(tfor));
	}

	return tfor;
}

static void decl_initialise_sue(decl_init ***init_iter,
		struct_union_enum_st *sue, expr *base, stmt *init_code)
{
	/* iterate over each member, pulling from the dinit */
	sue_member **smem;
	decl_init *dinit = **init_iter;
	decl_init **sue_iter;
	int cnt;

	if(dinit == NULL)
		ICE("TODO: null dinit for struct");

	sue_iter = (dinit->type == decl_init_scalar ? *init_iter : dinit->bits.inits);

	for(smem = sue->members, cnt = 0;
			smem && *smem;
			smem++, cnt++)
	{
		decl *const sue_mem = (*smem)->struct_member;

		/* room for optimisation below - avoid sue name lookup */
		expr *accessor = expr_new_struct(base, 1 /* a.b */,
				expr_new_identifier(sue_mem->spel));

		decl_init_create_assignments_discard(
				&sue_iter,
				sue_mem->ref,
				accessor,
				init_code);
	}

	*init_iter += cnt;
}

static void decl_initialise_scalar(
		decl_init ***init_iter, expr *base, stmt *init_code)
{
	decl_init *const dinit = **init_iter;
	expr *assign_from, *assign_init;

	if(dinit){
		if(dinit->type == decl_init_brace){
			/* initialising scalar with { ... } - pick first */
			decl_init **inits = dinit->bits.inits;

			if(inits && inits[1])
				WARN_AT(&inits[1]->where, "excess initaliser%s", inits[2] ? "s" : "");

			/* this seems to be called when it shouldn't... */
			decl_initialise_scalar(&inits, base, init_code);
			goto fin;
		}

		assert(dinit->type == decl_init_scalar);
		assign_from = dinit->bits.expr;
	}else{
		assign_from = expr_new_val(0);
	}

	assign_init = expr_new_assign(base, assign_from);
	assign_init->assign_is_init = 1;

	dynarray_add((void ***)&init_code->codes,
			expr_to_stmt(assign_init, init_code->symtab));

	if(dinit)
fin: ++*init_iter; /* we've used this init */
}

static type_ref *decl_init_create_assignments(
		decl_init ***init_iter,
		type_ref *const tfor_wrapped, /* could be typedef/cast */
		expr *base,
		stmt *init_code)
{
	/* iterate over tfor's array/struct members/scalar,
	 * pulling from dinit as necessary */
	type_ref *tfor, *tfor_ret = tfor_wrapped;
	struct_union_enum_st *sue;

	if((tfor = type_ref_is(tfor_wrapped, type_ref_array))){
		tfor_ret = decl_initialise_array(init_iter, tfor, base, init_code);

	}else if((sue = type_ref_is_s_or_u(tfor_wrapped))){
		decl_initialise_sue(init_iter, sue, base, init_code);

	}else{
		decl_initialise_scalar(init_iter, base, init_code);
	}

	return tfor_ret;
}

static void decl_init_create_assignments_discard(
		decl_init ***init_iter, type_ref *const tfor_wrapped,
		expr *base, stmt *init_code)
{
	type_ref *t = decl_init_create_assignments(init_iter, tfor_wrapped, base, init_code);

	if(t != tfor_wrapped)
		type_ref_free_1(t);
}

static type_ref *decl_init_create_assignments_from_init(
		decl_init *single_init,
		type_ref *const tfor_wrapped, /* could be typedef/cast */
		expr *base,
		stmt *init_code)
{
	decl_init *ar[] = { single_init, NULL };
	decl_init **it = ar;
	struct_union_enum_st *sue;

	/* init validity checks */
	if(single_init->type == decl_init_scalar){
		if((sue = type_ref_is_s_or_u(tfor_wrapped))
		||        type_ref_is(       tfor_wrapped, type_ref_array))
		{
			DIE_AT(&single_init->where, "%s must be initalised with an initialiser list",
					sue ? sue_str(sue) : "array");
		}
	}


	return decl_init_create_assignments(
			&it, tfor_wrapped, base, init_code);
}

void decl_init_create_assignments_for_spel(decl *d, stmt *init_code)
{
	d->ref = decl_init_create_assignments_from_init(
			d->init, d->ref,
			expr_new_identifier(d->spel), init_code);
}

void decl_init_create_assignments_for_base(decl *d, expr *base, stmt *init_code)
{
	d->ref = decl_init_create_assignments_from_init(
			d->init, d->ref, base, init_code);
}
