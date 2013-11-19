#include "ops.h"
#include "expr_block.h"
#include "../out/lbl.h"
#include "../../util/dynarray.h"
#include "../funcargs.h"

const char *str_expr_block(void)
{
	return "block";
}

void expr_block_set_ty(decl *db, type_ref *retty)
{
	expr *e = db->block_expr;

	db->ref = type_ref_new_block(
			type_ref_new_func(retty, e->bits.block.args),
			qual_const);
}

/*
 * TODO:
 * search e->code for expr_identifier,
 * and it it's not in e->code->symtab or lower,
 * add it as a block argument
 *
 * int i;
 * ^{
 *   i = 5;
 * }();
 */

void fold_expr_block(expr *e, symtable *scope_stab)
{
	/* prevent access to nested vars */
	symtable *const arg_symtab = e->code->symtab;
	symtable *const sym_root = symtab_root(arg_symtab);
	decl *df = decl_new();

	(void)scope_stab;

	symtab_func_root(arg_symtab)->in_func = df;

	/* add a global symbol for the block */
	e->bits.block.sym = sym_new_stab(sym_root, df, sym_global);

	/* fold block code (needs to be done before return-type of block) */
	UCC_ASSERT(stmt_kind(e->code, code), "!code for block");

	df->spel = out_label_block("globl");
	df->func_code = e->code;
	df->block_expr = e;

	if(e->bits.block.retty){
		expr_block_set_ty(df, e->bits.block.retty);
	}else{
		/* df->ref is NULL until we find a return statement,
		 * which sets df->ref for us */
	}

	fold_funcargs(e->bits.block.args, arg_symtab, NULL);
	fold_decl(df, arg_symtab, /*pinitcode:*/NULL);
	fold_func_code(df, arg_symtab);

	/* if we didn't hit any returns, we're a void block */
	if(!df->ref)
		expr_block_set_ty(df, type_ref_cached_VOID());

	e->tree_type = df->ref;

	fold_func_passable(df, type_ref_func_call(e->tree_type, NULL));
}

void gen_expr_block(expr *e)
{
	out_push_sym(e->bits.block.sym);
}

void gen_expr_str_block(expr *e)
{
	idt_printf("block, type: %s, code:\n", type_ref_to_str(e->tree_type));
	gen_str_indent++;
	print_stmt(e->code);
	gen_str_indent--;
}

void gen_expr_style_block(expr *e)
{
	stylef("^%s", type_ref_to_str(e->tree_type));
	gen_stmt(e->code);
}

void mutate_expr_block(expr *e)
{
	(void)e;
}

expr *expr_new_block(type_ref *rt, funcargs *args, stmt *code)
{
	expr *e = expr_new_wrapper(block);
	e->code = code;
	e->bits.block.args = args;
	e->bits.block.retty = rt; /* return type if not null */
	return e;
}
