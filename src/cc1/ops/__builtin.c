#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../../util/util.h"
#include "../../util/dynarray.h"
#include "../../util/alloc.h"

#include "../data_structs.h"
#include "__builtin.h"

#include "../cc1.h"
#include "../tokenise.h"
#include "../parse.h"
#include "../fold.h"

#include "../const.h"
#include "../gen_asm.h"
#include "../gen_str.h"

/* for asm_temp() */
#include "../asm.h"

#define PREFIX "__builtin_"

typedef expr *func_builtin_parse(void);

static func_builtin_parse parse_unreachable,
													parse_compatible_p,
													parse_constant_p,
													parse_frame_address,
													parse_expect;

typedef struct
{
	const char *sp;
	func_builtin_parse *parser;
} builtin_table;

builtin_table builtins[] = {
	{ "unreachable", parse_unreachable },
	{ "trap", parse_unreachable }, /* same */

	{ "types_compatible_p", parse_compatible_p },
	{ "constant_p", parse_constant_p },

	{ "frame_address", parse_frame_address },

	{ "expect", parse_expect },

	{ NULL, NULL }
};

static builtin_table *builtin_find(const char *sp)
{
	const int prefix_len = strlen(PREFIX);

	if(!strncmp(sp, PREFIX, prefix_len)){
		int i;
		sp += prefix_len;

		for(i = 0; builtins[i].sp; i++)
			if(!strcmp(sp, builtins[i].sp))
				return &builtins[i];
	}

	return NULL;
}

expr *builtin_parse(const char *sp)
{
	builtin_table *b = builtin_find(sp);

	if(b){
		expr *(*f)(void) = b->parser;

		if(f){
			expr *e = f();
			e->spel = ustrdup(sp);
			return e;
		}
	}

	return NULL;
}

static void gen_str_builtin(expr *e, symtable *stab)
{
	const enum pdeclargs dflags =
		  PDECL_INDENT
		| PDECL_NEWLINE
		| PDECL_SYM_OFFSET
		| PDECL_FUNC_DESCEND
		| PDECL_PISDEF
		| PDECL_PINIT
		| PDECL_SIZE
		| PDECL_ATTR;

	(void)stab;
	idt_printf("%s(\n", e->spel);

#define PRINT_ARGS(type, from, func)      \
	{                                       \
		type **i;                             \
                                          \
		gen_str_indent++;                     \
		for(i = from; i && *i; i++){          \
			func;                               \
			if(i[1])                            \
				idt_printf(",\n");                \
		}                                     \
		gen_str_indent--;                     \
	}

	PRINT_ARGS(expr, e->funcargs,            print_expr(*i))

	if(e->block_args)
		PRINT_ARGS(decl, e->block_args->arglist, print_decl(*i, dflags))

	idt_printf(");\n");
}

#define F_GEN(exp, fc) if(cc1_backend == BACKEND_ASM) exp->f_gen = fc

#define expr_mutate_builtin(exp, to)  \
	exp->f_fold = fold_ ## to,          \
	exp->f_gen        = gen_str_builtin

#define expr_mutate_builtin_const(exp, to) \
	expr_mutate_builtin(exp, to),             \
	exp->f_const_fold = const_ ## to

static void wur_builtin(expr *e)
{
	e->freestanding = 0; /* needs use */
}

static void builtin_gen_undefined(expr *e, symtable *stab)
{
	(void)e;
	(void)stab;
	asm_temp(1, "ud2 ; undefined");
}

static expr *parse_any_args(void)
{
	expr *fcall = expr_new_funcall();
	fcall->funcargs = parse_funcargs();
	return fcall;
}

/* --- unreachable */

static void fold_unreachable(expr *e, symtable *stab)
{
	(void)stab;

	e->tree_type = decl_new_void();
	decl_attr_append(&e->tree_type->attr, decl_attr_new(attr_noreturn));

	wur_builtin(e);
}

static expr *parse_unreachable(void)
{
	expr *fcall = expr_new_funcall();

	expr_mutate_builtin(fcall, unreachable);
	F_GEN(fcall, builtin_gen_undefined);

	return fcall;
}

/* --- compatible_p */

static void fold_compatible_p(expr *e, symtable *stab)
{
	decl **types = e->block_args->arglist;

	if(dynarray_count((void **)types) != 2)
		DIE_AT(&e->where, "need two arguments for %s", e->expr->spel);

	fold_decl(types[0], stab);
	fold_decl(types[1], stab);

	e->tree_type = decl_new_int();
	wur_builtin(e);
}

static void const_compatible_p(expr *e, intval *val, enum constyness *success)
{
	decl **types = e->block_args->arglist;

	*success = CONST_WITH_VAL;

	val->val = decl_equal(types[0], types[1], DECL_CMP_STRICT_PRIMITIVE);
}

static expr *parse_compatible_p(void)
{
	expr *fcall = expr_new_funcall();

	fcall->block_args = funcargs_new();
	fcall->block_args->arglist = parse_type_list();

	expr_mutate_builtin_const(fcall, compatible_p);

	return fcall;
}

/* --- constant */

static void fold_constant_p(expr *e, symtable *stab)
{
	if(dynarray_count((void **)e->funcargs) != 1)
		DIE_AT(&e->where, "%s takes a single argument", e->expr->spel);

	fold_expr(e->funcargs[0], stab);

	e->tree_type = decl_new_int();
	wur_builtin(e);
}

static void const_constant_p(expr *e, intval *val, enum constyness *success)
{
	expr *test = *e->funcargs;
	enum constyness type;
	intval iv;

	const_fold(test, &iv, &type);

	*success = CONST_WITH_VAL;
	val->val = type != CONST_NO;
}

static expr *parse_constant_p(void)
{
	expr *fcall = parse_any_args();
	expr_mutate_builtin_const(fcall, constant_p);
	return fcall;
}

/* --- frame_address */

static void fold_frame_address(expr *e, symtable *stab)
{
	enum constyness type;
	intval iv;

	if(dynarray_count((void **)e->funcargs) != 1)
		DIE_AT(&e->where, "%s takes a single argument", e->expr->spel);

	fold_expr(e->funcargs[0], stab);

	const_fold(e->funcargs[0], &iv, &type);
	if(type != CONST_WITH_VAL || iv.val < 0)
		DIE_AT(&e->where, "%s needs a positive constant value argument", e->expr->spel);

	memcpy(&e->bits.iv, &iv, sizeof iv);

	e->tree_type = decl_ptr_depth_inc(decl_new_void());
	wur_builtin(e);
}

static void builtin_gen_frame_pointer(expr *e, symtable *stab)
{
	int depth = e->bits.iv.val;

	(void)stab;

	asm_temp(1, "mov rax, rbp");
	while(--depth > 0)
		asm_temp(1, "mov rax, [rax]");
	asm_temp(1, "push rax");
}

static expr *parse_frame_address(void)
{
	expr *fcall = parse_any_args();
	expr_mutate_builtin(fcall, frame_address);
	F_GEN(fcall, builtin_gen_frame_pointer);
	return fcall;
}

/* --- expect */

static void fold_expect(expr *e, symtable *stab)
{
	enum constyness type;
	intval iv;
	int i;

	if(dynarray_count((void **)e->funcargs) != 2)
		DIE_AT(&e->where, "%s takes two arguments", e->expr->spel);

	for(i = 0; i < 2; i++)
		fold_expr(e->funcargs[i], stab);

	const_fold(e->funcargs[1], &iv, &type);
	if(type != CONST_WITH_VAL)
		WARN_AT(&e->where, "%s second argument isn't a constant value", e->expr->spel);

	e->tree_type = decl_copy(e->funcargs[0]->tree_type);
	wur_builtin(e);
}

static void builtin_gen_expect(expr *e, symtable *stab)
{
	gen_expr(e->funcargs[1], stab); /* not needed if it's const, but gcc and clang do this */
	asm_temp(1, "pop rax ; unused __builtin_expect()");
	gen_expr(e->funcargs[0], stab);
}

static void const_expect(expr *e, intval *val, enum constyness *success)
{
	const_fold(e->funcargs[0], val, success);
}

static expr *parse_expect(void)
{
	expr *fcall = parse_any_args();
	expr_mutate_builtin_const(fcall, expect);

	F_GEN(fcall, builtin_gen_expect);

	return fcall;
}
