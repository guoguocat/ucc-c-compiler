#ifndef EXPR_ASSIGN_H
#define EXPR_ASSIGN_H

func_fold    fold_expr_assign;
func_gen     gen_expr_assign;
func_str     str_expr_assign;
func_gen     gen_expr_str_assign;
func_mutate_expr mutate_expr_assign;
func_gen     gen_expr_style_assign;

enum lvalue_opts
{
	LVAL_ALLOW_FUNC  = 1 << 0,
};

int expr_is_lvalue(expr *e, enum lvalue_opts);

#endif
