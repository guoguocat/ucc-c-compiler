#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "../../util/dynarray.h"
#include "../../util/platform.h"
#include "../../util/alloc.h"

#include "ops.h"
#include "expr_funcall.h"
#include "../funcargs.h"
#include "../format_chk.h"
#include "../type_is.h"
#include "../type_nav.h"

const char *str_expr_funcall()
{
	return "funcall";
}

static attribute *func_attr_present(expr *e, enum attribute_type t)
{
	attribute *a;
	a = expr_attr_present(e, t);
	if(a)
		return a;
	return expr_attr_present(e->expr, t);
}

static void sentinel_check(where *w, expr *e, expr **args,
		const int variadic, const int nstdargs, symtable *stab)
{
#define ATTR_WARN_RET(w, ...) do{ warn_at(w, __VA_ARGS__); return; }while(0)

	attribute *attr = func_attr_present(e, attr_sentinel);
	int i, nvs;
	expr *sentinel;

	if(!attr)
		return;

	if(!variadic)
		return; /* warning emitted elsewhere, on the decl */

	if(attr->bits.sentinel){
		consty k;

		FOLD_EXPR(attr->bits.sentinel, stab);
		const_fold(attr->bits.sentinel, &k);

		if(k.type != CONST_NUM || !K_INTEGRAL(k.bits.num))
			die_at(&attr->where, "sentinel attribute not reducible to integer constant");

		i = k.bits.num.val.i;
	}else{
		i = 0;
	}

	nvs = dynarray_count(args) - nstdargs;

	if(nvs == 0)
		ATTR_WARN_RET(w, "not enough variadic arguments for a sentinel");

	UCC_ASSERT(nvs >= 0, "too few args");

	if(i >= nvs)
		ATTR_WARN_RET(w, "sentinel index is not a variadic argument");

	sentinel = args[(nstdargs + nvs - 1) - i];

	/* must be of a pointer type, printf("%p\n", 0) is undefined */
	if(!expr_is_null_ptr(sentinel, NULL_STRICT_ANY_PTR))
		ATTR_WARN_RET(&sentinel->where, "sentinel argument expected (got %s)",
				type_to_str(sentinel->tree_type));

#undef ATTR_WARN_RET
}

static void static_array_check(
		decl *arg_decl, expr *arg_expr)
{
	/* if ty_func is x[static %d], check counts */
	type *ty_expr = arg_expr->tree_type;
	type *ty_decl = decl_is_decayed_array(arg_decl);
	consty k_decl;

	if(!ty_decl || !ty_decl->bits.ptr.is_static)
		return;

	/* want to check any pointer type */
	if(expr_is_null_ptr(arg_expr, NULL_STRICT_ANY_PTR)){
		warn_at(&arg_expr->where, "passing null-pointer where array expected");
		return;
	}

	if(!ty_decl->bits.ptr.size)
		return;

	const_fold(ty_decl->bits.ptr.size, &k_decl);

	if((ty_expr = type_is_decayed_array(ty_expr))){
		/* ty_expr is the type_ptr, decayed from array */
		if(ty_expr->bits.ptr.size){
			consty k_arg;

			const_fold(ty_expr->bits.ptr.size, &k_arg);

			if(k_decl.type == CONST_NUM
			&& K_INTEGRAL(k_arg.bits.num)
			&& k_arg.bits.num.val.i < k_decl.bits.num.val.i)
			{
				warn_at(&arg_expr->where,
						"array of size %" NUMERIC_FMT_D
						" passed where size %" NUMERIC_FMT_D " needed",
						k_arg.bits.num.val.i, k_decl.bits.num.val.i);
			}
		}
	}
	/* else it's a random pointer, just be quiet */
}

void fold_expr_funcall(expr *e, symtable *stab)
{
	type *func_ty;
	funcargs *args_from_decl;
	char *sp = NULL;
	int count_decl = 0;

#if 0
	if(func_is_asm(sp)){
		expr *arg1;
		const char *str;
		int i;

		if(!e->funcargs || e->funcargs[1] || !expr_kind(e->funcargs[0], addr))
			die_at(&e->where, "invalid __asm__ arguments");

		arg1 = e->funcargs[0];
		str = arg1->data_store->data.str;
		for(i = 0; i < arg1->array_store->len - 1; i++){
			char ch = str[i];
			if(!isprint(ch) && !isspace(ch))
invalid:
				die_at(&arg1->where, "invalid __asm__ string (character 0x%x at index %d, %d / %d)",
						ch, i, i + 1, arg1->array_store->len);
		}

		if(str[i])
			goto invalid;

		/* TODO: allow a long return, e.g. __asm__(("movq $5, %rax")) */
		e->tree_type = decl_new_void();
		return;
		ICE("TODO: __asm__");
	}
#endif


	if(!e->expr->in_parens && expr_kind(e->expr, identifier) && (sp = e->expr->bits.ident.spel)){
		/* check for implicit function */
		if(!(e->expr->bits.ident.sym = symtab_search(stab, sp))){
			funcargs *args = funcargs_new();
			decl *df;

			/* set up the funcargs as if it's "x()" - i.e. any args */
			funcargs_empty(args);

			func_ty = type_func_of(
					type_nav_btype(cc1_type_nav, type_int),
					args,
					symtab_new(stab, &e->where) /*new symtable for args*/);

			cc1_warn_at(&e->expr->where, 0, WARN_IMPLICIT_FUNC,
					"implicit declaration of function \"%s\"", sp);

			df = decl_new();
			df->ref = func_ty;
			df->spel = e->expr->bits.ident.spel;

			fold_decl(df, stab, NULL); /* update calling conv, for e.g. */

			df->sym->type = sym_global;

			e->expr->bits.ident.sym = df->sym;
		}
	}

	FOLD_EXPR(e->expr, stab);
	func_ty = e->expr->tree_type;

	if(!type_is_callable(func_ty)){
		die_at(&e->expr->where, "%s-expression (type '%s') not callable",
				e->expr->f_str(), type_to_str(func_ty));
	}

	if(expr_kind(e->expr, deref)
	&& type_is(type_is_ptr(expr_deref_what(e->expr)->tree_type), type_func)){
		/* XXX: memleak */
		/* (*f)() - dereffing to a function, then calling - remove the deref */
		e->expr = expr_deref_what(e->expr);
	}

	e->tree_type = type_func_call(func_ty, &args_from_decl);

	/* func count comparison, only if the func has arg-decls, or the func is f(void) */
	UCC_ASSERT(args_from_decl, "no funcargs for decl %s", sp);


	/* this block is purely count checking */
	if(args_from_decl->arglist || args_from_decl->args_void){
		const int count_arg  = dynarray_count(e->funcargs);

		count_decl = dynarray_count(args_from_decl->arglist);

		if(count_decl != count_arg && (args_from_decl->variadic ? count_arg < count_decl : 1)){
			die_at(&e->where, "too %s arguments to function %s (got %d, need %d)",
					count_arg > count_decl ? "many" : "few",
					sp, count_arg, count_decl);
		}
	}else if(args_from_decl->args_void_implicit && e->funcargs){
		warn_at(&e->where, "too many arguments to implicitly (void)-function");
	}

	/* this block folds the args and type-checks */
	if(e->funcargs){
		unsigned long nonnulls = 0;
		int i;
		attribute *da;
#define ARG_BUF(buf, i, sp)             \
				snprintf(buf, sizeof buf,       \
						"argument %d to %s",        \
						i + 1, sp ? sp : "function")

		char buf[64];

		if((da = func_attr_present(e, attr_nonnull)))
			nonnulls = da->bits.nonnull_args;

		for(i = 0; e->funcargs[i]; i++){
			expr *arg = FOLD_EXPR(e->funcargs[i], stab);

			ARG_BUF(buf, i, sp);

			fold_check_expr(arg, FOLD_CHK_NO_ST_UN, buf);

			if(i < count_decl && (nonnulls & (1 << i))
			&& type_is_ptr(args_from_decl->arglist[i]->ref)
			&& expr_is_null_ptr(arg, NULL_STRICT_INT))
			{
				warn_at(&arg->where, "null passed where non-null required (arg %d)",
						i + 1);
			}
		}
	}

	/* this block is purely type checking */
	if(args_from_decl->arglist || args_from_decl->args_void){
		int count_arg;

		count_arg  = dynarray_count(e->funcargs);
		count_decl = dynarray_count(args_from_decl->arglist);

		if(count_decl != count_arg && (args_from_decl->variadic ? count_arg < count_decl : 1)){
			die_at(&e->where, "too %s arguments to function %s (got %d, need %d)",
					count_arg > count_decl ? "many" : "few",
					sp, count_arg, count_decl);
		}

		if(e->funcargs){
			int i;
			char buf[64];

			for(i = 0; ; i++){
				decl *decl_arg = args_from_decl->arglist[i];

				if(!decl_arg)
					break;

				ARG_BUF(buf, i, sp);

				fold_type_chk_and_cast(
						decl_arg->ref, &e->funcargs[i],
						stab, &e->funcargs[i]->where,
						buf);

				/* f(int [static 5]) check */
				static_array_check(decl_arg, e->funcargs[i]);
			}
		}
	}

	/* each unspecified arg needs default promotion, (if smaller) */
	if(e->funcargs){
		int i;
		for(i = count_decl; e->funcargs[i]; i++)
			expr_promote_default(&e->funcargs[i], stab);
	}

	if(type_is_s_or_u(e->tree_type))
		ICW("TODO: function returning a struct");

	/* attr */
	{
		type *r = e->expr->tree_type;

		format_check_call(
				&e->where, r,
				e->funcargs, args_from_decl->variadic);

		sentinel_check(
				&e->where, e,
				e->funcargs, args_from_decl->variadic,
				count_decl, stab);
	}

	/* check the subexp tree type to get the funcall attributes */
	if(func_attr_present(e, attr_warn_unused))
		e->freestanding = 0; /* needs use */
}

void gen_expr_funcall(expr *e)
{
	if(0){
		out_comment("start manual __asm__");
		ICE("same");
#if 0
		fprintf(cc_out[SECTION_TEXT], "%s\n", e->funcargs[0]->data_store->data.str);
#endif
		out_comment("end manual __asm__");
	}else{
		/* continue with normal funcall */
		int nargs = 0;

		gen_expr(e->expr);

		if(e->funcargs){
			expr **aiter;

			for(aiter = e->funcargs; *aiter; aiter++, nargs++);

			for(aiter--; aiter >= e->funcargs; aiter--){
				expr *earg = *aiter;

				/* should be of size int or larger (for integral types)
				 * or double (for floating types)
				 */
				gen_expr(earg);
			}
		}

		out_call(nargs, e->tree_type, e->expr->tree_type);
	}
}

void gen_expr_str_funcall(expr *e)
{
	expr **iter;

	idt_printf("funcall, calling:\n");

	gen_str_indent++;
	print_expr(e->expr);
	gen_str_indent--;

	if(e->funcargs){
		int i;
		idt_printf("args:\n");
		gen_str_indent++;
		for(i = 1, iter = e->funcargs; *iter; iter++, i++){
			idt_printf("arg %d:\n", i);
			gen_str_indent++;
			print_expr(*iter);
			gen_str_indent--;
		}
		gen_str_indent--;
	}else{
		idt_printf("no args\n");
	}
}

void mutate_expr_funcall(expr *e)
{
	(void)e;
}

int expr_func_passable(expr *e)
{
	/* need to check the sub-expr, i.e. the function */
	return !func_attr_present(e, attr_noreturn);
}

expr *expr_new_funcall()
{
	expr *e = expr_new_wrapper(funcall);
	e->freestanding = 1;
	return e;
}

void gen_expr_style_funcall(expr *e)
{
	stylef("(");
	gen_expr(e->expr);
	stylef(")(");
	if(e->funcargs){
		expr **i;
		for(i = e->funcargs; i && *i; i++){
			gen_expr(*i);
			if(i[1])
				stylef(", ");
		}
	}
	stylef(")");
}
