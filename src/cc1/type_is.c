#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "../util/where.h"
#include "../util/util.h"
#include "../util/platform.h"

#include "expr.h"
#include "sue.h"
#include "type.h"
#include "type_nav.h"
#include "decl.h"
#include "const.h"
#include "funcargs.h"

#include "type_is.h"

static type *type_next_1(type *r)
{
	if(r->type == type_tdef){
		/* typedef - jump to its typeof */
		struct type_tdef *tdef = &r->bits.tdef;
		decl *preferred = tdef->decl;

		r = preferred ? preferred->ref : tdef->type_of->tree_type;

		UCC_ASSERT(r, "unfolded typeof()");

		return r;
	}

	return r->ref;
}

enum type_skippage
{
	STOP_AT_TDEF = 1 << 0,
	STOP_AT_CAST = 1 << 1,
	STOP_AT_ATTR = 1 << 2,
	STOP_AT_WHERE = 1 << 3,
};
static type *type_skip(type *t, enum type_skippage skippage)
{
	while(t){
		switch(t->type){
			case type_tdef:
				if(skippage & STOP_AT_TDEF)
					goto fin;
				break;
			case type_cast:
				if(skippage & STOP_AT_CAST)
					goto fin;
				break;
			case type_attr:
				if(skippage & STOP_AT_ATTR)
					goto fin;
				break;
			case type_where:
				if(skippage & STOP_AT_WHERE)
					goto fin;
				break;
			default:
				goto fin;
		}
		t = type_next_1(t);
	}

fin:
	return t;
}

type *type_skip_all(type *t)
{
	return type_skip(t, 0);
}

type *type_skip_non_tdefs(type *t)
{
	return type_skip(t, STOP_AT_TDEF);
}

type *type_skip_non_casts(type *t)
{
	return type_skip(t, STOP_AT_CAST);
}

type *type_skip_wheres(type *t)
{
	return type_skip(t, ~0 & ~STOP_AT_WHERE);
}

type *type_skip_non_wheres(type *t)
{
	return type_skip(t, STOP_AT_WHERE);
}

decl *type_is_tdef(type *t)
{
	t = type_skip_non_tdefs(t);

	if(t && t->type == type_tdef)
		return t->bits.tdef.decl;

	return NULL;
}

type *type_next(type *r)
{
	if(!r)
		return NULL;

	switch(r->type){
		case type_btype:
			return NULL;

		case type_tdef:
		case type_cast:
		case type_attr:
		case type_where:
			return type_next(type_skip_all(r));

		case type_ptr:
		case type_block:
		case type_func:
		case type_array:
			return r->ref;
	}

	ucc_unreach(NULL);
}

type *type_is(type *r, enum type_kind t)
{
	r = type_skip_all(r);

	if(!r || r->type != t)
		return NULL;

	return r;
}

type *type_is_primitive(type *r, enum type_primitive p)
{
	r = type_is(r, type_btype);

	/* extra checks for a type */
	if(r && (p == type_unknown || r->bits.type->primitive == p))
		return r;

	return NULL;
}

type *type_is_ptr(type *r)
{
	r = type_is(r, type_ptr);
	return r ? r->ref : NULL;
}

type *type_is_ptr_or_block(type *r)
{
	type *t = type_is_ptr(r);

	return t ? t : type_is(r, type_block);
}

type *type_is_array(type *r)
{
	r = type_is(r, type_array);
	return r ? r->ref : NULL;
}

type *type_is_scalar(type *r)
{
	if(type_is_s_or_u(r) || type_is_array(r))
		return NULL;
	return r;
}

type *type_is_func_or_block(type *r)
{
	type *t = type_is(r, type_func);
	if(t)
		return t;

	t = type_is(r, type_block);
	if(t){
		t = type_skip_all(type_next(t));
		UCC_ASSERT(t->type == type_func,
				"block->next not func?");
		return t;
	}

	return NULL;
}

const btype *type_get_type(type *t)
{
	t = type_skip_all(t);
	return t && t->type == type_btype ? t->bits.type : NULL;
}

int type_is_bool(type *r)
{
	if(type_is(r, type_ptr))
		return 1;

	r = type_is(r, type_btype);

	if(!r)
		return 0;

	return type_is_integral(r);
}

int type_is_fptr(type *r)
{
	return !!type_is(type_is_ptr(r), type_func);
}

int type_is_nonfptr(type *r)
{
	if((r = type_is_ptr(r)))
		return !type_is(r, type_func);

	return 0; /* not a ptr */
}

int type_is_void_ptr(type *r)
{
	return !!type_is_primitive(type_is_ptr(r), type_void);
}

int type_is_nonvoid_ptr(type *r)
{
	if((r = type_is_ptr(r)))
		return !type_is_primitive(r, type_void);
	return 0;
}

int type_is_integral(type *r)
{
	r = type_is(r, type_btype);

	if(!r)
		return 0;

	switch(r->bits.type->primitive){
		case type_int:   case type_uint:
		case type_nchar: case type_schar: case type_uchar:
		case type__Bool:
		case type_short: case type_ushort:
		case type_long:  case type_ulong:
		case type_llong: case type_ullong:
		case type_enum:
			return 1;

		case type_unknown:
		case type_void:
		case type_struct:
		case type_union:
		case type_float:
		case type_double:
		case type_ldouble:
			break;
	}

	return 0;
}

int type_is_complete(type *r)
{
	/* decl is "void" or incomplete-struct or array[] */
	r = type_skip_all(r);

	switch(r->type){
		case type_btype:
		{
			const btype *t = r->bits.type;

			switch(t->primitive){
				case type_void:
					return 0;
				case type_struct:
				case type_union:
				case type_enum:
					return sue_complete(t->sue);

				default:break;
			}

			break;
		}

		case type_array:
			return r->bits.array.size && type_is_complete(r->ref);

		case type_func:
		case type_ptr:
		case type_block:
			break;

		case type_tdef:
		case type_attr:
		case type_cast:
		case type_where:
			ICE("should've been skipped");
	}


	return 1;
}

int type_is_variably_modified(type *r)
{
	/* vlas not implemented yet */
#if 0
	if(type_is_array(r)){
		/* ... */
	}
#else
	(void)r;
#endif
	return 0;
}

int type_is_incomplete_array(type *r)
{
	if((r = type_is(r, type_array)))
		return !r->bits.array.size;

	return 0;
}

type *type_complete_array(type *r, expr *sz)
{
	r = type_is(r, type_array);

	UCC_ASSERT(r, "not an array");

	r = type_array_of(r->ref, sz);

	return r;
}

struct_union_enum_st *type_is_s_or_u_or_e(type *r)
{
	type *test = type_is(r, type_btype);

	if(!test)
		return NULL;

	return test->bits.type->sue; /* NULL if not s/u/e */
}

struct_union_enum_st *type_is_s_or_u(type *r)
{
	struct_union_enum_st *sue = type_is_s_or_u_or_e(r);
	if(sue && sue->primitive != type_enum)
		return sue;
	return NULL;
}

type *type_func_call(type *fp, funcargs **pfuncargs)
{
	fp = type_skip_all(fp);
	switch(fp->type){
		case type_ptr:
		case type_block:
			fp = type_is(fp->ref, type_func);
			UCC_ASSERT(fp, "not a func for fcall");
			/* fall */

		case type_func:
			if(pfuncargs)
				*pfuncargs = fp->bits.func.args;
			fp = fp->ref;
			UCC_ASSERT(fp, "no ref for func");
			break;

		default:
			ICE("can't func-deref non func-ptr/block ref (%d)", fp->type);
	}

	return fp;
}

int type_decayable(type *r)
{
	switch(type_skip_all(r)->type){
		case type_array:
		case type_func:
			return 1;
		default:
			return 0;
	}
}

static type *type_keep_w_attr(type *t, where *loc, attribute *attr)
{
	if(loc && !type_has_loc(t))
		t = type_at_where(t, loc);

	return type_attributed(t, RETAIN(attr));
}

type *type_decay(type *const ty)
{
	/* f(int x[][5]) decays to f(int (*x)[5]), not f(int **x) */
	where *loc = NULL;
	attribute *attr = NULL;
	type *test;

	for(test = ty; test; test = type_next_1(test)){
		switch(test->type){
			case type_where:
				if(!loc)
					loc = &test->bits.where;
				break;

			case type_attr:
				if(!attr)
					attr = test->bits.attr;
				break;

			case type_cast:
			case type_tdef:
				/* skip */
				break;

			case type_btype:
			case type_ptr:
			case type_block:
				/* nothing to decay */
				return ty;

			case type_array:
				return type_keep_w_attr(
						type_decayed_ptr_to(test->ref, test),
						loc, attr);

			case type_func:
				return type_keep_w_attr(
						type_ptr_to(test),
						loc, attr);
		}
	}

	return ty;
}

int type_is_void(type *r)
{
	return !!type_is_primitive(r, type_void);
}

int type_is_signed(type *r)
{
	/* need to take casts into account */
	while(r)
		switch(r->type){
			case type_btype:
				return btype_is_signed(r->bits.type);

			case type_ptr:
				/* "unspecified" */
				return 1;

			case type_cast:
				if(r->bits.cast.is_signed_cast)
					return r->bits.cast.signed_true;
				/* fall */

			default:
				r = type_next_1(r);
		}

	return 0;
}

int type_is_floating(type *r)
{
	r = type_is(r, type_btype);

	if(!r)
		return 0;

	return type_floating(r->bits.type->primitive);
}

enum type_qualifier type_qual(const type *r)
{
	/* stop at the first pointer or type, collecting from type_cast quals */

	if(!r)
		return qual_none;

	switch(r->type){
		case type_btype:
			if(r->bits.type->primitive == type_struct
			|| r->bits.type->primitive == type_union)
			{
				if(r->bits.type->sue->contains_const)
					return qual_const;
			}

		case type_func:
		case type_array:
			return qual_none;

		case type_where:
		case type_attr:
			return type_qual(r->ref);

		case type_cast:
			/* descend */
			if(r->bits.cast.is_signed_cast)
				return type_qual(r->ref);
			return r->bits.cast.qual | type_qual(r->ref);

		case type_ptr:
		case type_block:
			return qual_none; /* no descend */

		case type_tdef:
			return type_qual(r->bits.tdef.type_of->tree_type);
	}

	ucc_unreach(qual_none);
}

enum type_primitive type_primitive(type *ty)
{
	ty = type_is_primitive(ty, type_unknown);
	UCC_ASSERT(ty, "not primitive?");
	return ty->bits.type->primitive;
}

funcargs *type_funcargs(type *r)
{
	type *test;

	r = type_skip_all(r);

	if((test = type_is(r, type_ptr))
	|| (test = type_is(r, type_block)))
	{
		r = type_skip_all(test->ref); /* jump down past the (*)() */
	}

	UCC_ASSERT(r && r->type == type_func,
			"not a function type - %s",
			type_kind_to_str(r->type));

	return r->bits.func.args;
}

int type_is_callable(type *r)
{
	type *test;

	r = type_skip_all(r);

	if((test = type_is(r, type_ptr)) || (test = type_is(r, type_block)))
		return !!type_is(test->ref, type_func);

	return 0;
}

int type_is_const(type *r)
{
	/* const char *x is not const. char *const x is */
	return !!(type_qual(r) & qual_const);
}

unsigned type_array_len(type *r)
{
	r = type_is(r, type_array);

	UCC_ASSERT(r, "not an array");
	UCC_ASSERT(r->bits.array.size, "array len of []");

	return const_fold_val_i(r->bits.array.size);
}

int type_is_promotable(type *r, type **pto)
{
	if((r = type_is_primitive(r, type_unknown))){
		static unsigned sz_int, sz_double;
		const int fp = type_floating(r->bits.type->primitive);
		unsigned rsz;

		if(!sz_int){
			sz_int = type_primitive_size(type_int);
			sz_double = type_primitive_size(type_double);
		}

		rsz = type_primitive_size(r->bits.type->primitive);

		if(rsz < (fp ? sz_double : sz_int)){
			*pto = type_nav_btype(cc1_type_nav, fp ? type_double : type_int);
			return 1;
		}
	}

	return 0;
}

int type_is_variadic_func(type *r)
{
	return (r = type_is(r, type_func)) && r->bits.func.args->variadic;
}

type *type_is_decayed_array(type *r)
{
	if((r = type_is(r, type_ptr)) && r->bits.ptr.decayed)
		return r;

	return NULL;
}
