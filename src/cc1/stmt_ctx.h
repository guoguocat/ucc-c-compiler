#ifndef STMT_CTX_H
#define STMT_CTX_H

struct dynmap;

typedef struct stmt_fold_ctx_function
{
	basic_blk *blk_return;
	struct dynmap *gotos; /* char * => basic_blk * */
} stmt_fold_ctx_function;

struct stmt_fold_ctx_block
{
	stmt_fold_ctx_function *func_ctx;

	stmt *curswitch;

	basic_blk *blk_break, *blk_continue;
};

/* child is a value, parent, pointer. type checking, yo */
#define STMT_CTX_NEST(child, parent) \
	child.func_ctx = parent->func_ctx;

#endif
