#ifndef BLK_H
#define BLK_H

func_block blockify_expr;
func_block_stmt blockify_stmt;

void blockify_func(decl *func);

void blockify_stmt_switch_lbl(stmt *s, stmt_fold_ctx_block *ctx);

#endif
