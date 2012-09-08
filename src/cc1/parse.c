#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../util/util.h"
#include "data_structs.h"
#include "tokenise.h"
#include "parse.h"
#include "../util/alloc.h"
#include "tokconv.h"
#include "../util/util.h"
#include "sym.h"
#include "cc1.h"
#include "../util/dynarray.h"
#include "sue.h"
#include "parse_type.h"
#include "data_store.h"
#include "const.h"

#define STAT_NEW(type)      stmt_new_wrapper(type, current_scope)
#define STAT_NEW_NEST(type) stmt_new_wrapper(type, symtab_new(current_scope))

stmt *parse_stmt_block(void);
stmt *parse_stmt(void);
expr **parse_funcargs(void);

expr *parse_expr_unary(void);
#define parse_expr_cast() parse_expr_unary()

/* parse_type uses this for structs, tdefs and enums */
symtable *current_scope;
static_assert **static_asserts;


/* sometimes we can carry on after an error, but we don't want to go through to compilation etc */
int parse_had_error = 0;


/* switch, for, do and while linkage */
static stmt *current_continue_target,
						*current_break_target,
						*current_switch;


expr *parse_expr_sizeof_typeof(int is_typeof)
{
	expr *e;

	if(accept(token_open_paren)){
		decl *d = parse_decl_single(DECL_SPEL_NO);

		if(d){
			e = expr_new_sizeof_decl(d, is_typeof);
		}else{
			/* parse a full one, since we're in brackets */
			e = expr_new_sizeof_expr(parse_expr_exp(), is_typeof);
		}

		EAT(token_close_paren);
	}else{
		if(is_typeof)
			/* TODO? cc1_error = 1, return expr_new_val(0) */
			DIE_AT(NULL, "open paren expected after typeof");

		e = expr_new_sizeof_expr(parse_expr_unary(), is_typeof);
		/* don't go any higher, sizeof a - 1, means sizeof(a) - 1 */
	}

	return e;
}

expr *parse_expr__Generic()
{
	struct generic_lbl **lbls;
	expr *test;

	EAT(token__Generic);
	EAT(token_open_paren);

	test = parse_expr_no_comma();
	lbls = NULL;

	for(;;){
		decl *d;
		expr *e;
		struct generic_lbl *lbl;

		EAT(token_comma);

		if(accept(token_default)){
			d = NULL;
		}else{
			d = parse_decl_single(DECL_SPEL_NO);
			if(!d)
				DIE_AT(NULL, "type expected");
		}
		EAT(token_colon);
		e = parse_expr_no_comma();

		lbl = umalloc(sizeof *lbl);
		lbl->e = e;
		lbl->d = d;
		dynarray_add((void ***)&lbls, lbl);

		if(accept(token_close_paren))
			break;
	}

	return expr_new__Generic(test, lbls);
}

expr *parse_expr_identifier()
{
	expr *e;

	if(curtok != token_identifier)
		DIE_AT(NULL, "identifier expected, got %s (%s:%d)",
				token_to_str(curtok), __FILE__, __LINE__);

	e = expr_new_identifier(token_current_spel());
	EAT(token_identifier);
	return e;
}

expr *parse_block()
{
	funcargs *args;
	decl *rt;

	EAT(token_xor);

	rt = parse_decl_single(DECL_SPEL_NO);

	if(rt){
		if(decl_is_func(rt)){
			/* got ^int (args...) */
			rt = decl_func_deref(rt, &args);
		}else{
			/* ^int {...} */
			goto def_args;
		}

	}else if(accept(token_open_paren)){
		/* ^(args...) */
		args = parse_func_arglist();
		EAT(token_close_paren);
	}else{
		/* ^{...} */
def_args:
		args = funcargs_new();
		args->args_void = 1;
	}

	return expr_new_block(rt, args, parse_stmt_block());
}

expr *parse_expr_primary()
{
	switch(curtok){
		case token_integer:
		case token_character:
		{
			expr *e = expr_new_intval(&currentval);
			EAT(curtok);
			return e;
		}

		case token_string:
		/*case token_open_block: - not allowed here */
		{
			char *s;
			int l;

			token_get_current_str(&s, &l);
			EAT(token_string);

			return expr_new_addr_data(data_store_new_str(s, l));
		}

		case token__Generic:
			return parse_expr__Generic();

		case token_xor:
			return parse_block();

		default:
			if(accept(token_open_paren)){
				decl *d;
				expr *e;

				if((d = parse_decl_single(DECL_SPEL_NO))){
					if(d->type->store)
						DIE_AT(&d->where, "invalid cast involving %s",
								type_store_to_str(d->type->store));

					e = expr_new_cast(d, 0);
					EAT(token_close_paren);
					e->expr = parse_expr_cast(); /* another cast */
					return e;

				}else if(curtok == token_open_block){
					/* ({ ... }) */
					e = expr_new_stmt(parse_stmt_block());
				}else{
					/* mark as being inside parens, for if((x = 5)) checking */
					e = parse_expr_exp();
					e->in_parens = 1;
				}

				EAT(token_close_paren);
				return e;
			}else{
				if(curtok != token_identifier){
					/* TODO? cc1_error = 1, return expr_new_val(0) */
					DIE_AT(NULL, "expression expected, got %s (%s:%d)",
							token_to_str(curtok), __FILE__, __LINE__);
				}

				return parse_expr_identifier();
			}
			break;
	}
}

expr *parse_expr_postfix()
{
	expr *e;
	int flag;

	e = parse_expr_primary();

	for(;;){
		if(accept(token_open_square)){
			expr *sum;

			sum = expr_new_op(op_plus);

			sum->lhs  = e;
			sum->rhs  = parse_expr_exp();

			EAT(token_close_square);

			e = expr_new_deref(sum);

		}else if(accept(token_open_paren)){
			expr *fcall = expr_new_funcall();

			fcall->funcargs = parse_funcargs();
			fcall->expr = e;
			EAT(token_close_paren);

			e = fcall;

		}else if((flag = accept(token_dot)) || accept(token_ptr)){
			e = expr_new_struct(e, flag, parse_expr_identifier());

		}else if((flag = accept(token_increment)) || accept(token_decrement)){
			e = expr_new_assign_compound(e, expr_new_val(1), flag ? op_plus : op_minus);
			e->assign_is_post = 1;

		}else{
			break;
		}
	}

	return e;
}

expr *parse_expr_unary()
{
	expr *e;
	int flag;

	if((flag = accept(token_increment)) || accept(token_decrement)){
		/* this is a normal increment, i.e. ++x, simply translate it to x += 1 */

		e = expr_new_assign_compound(
				parse_expr_unary(), /* lval */
				expr_new_val(1),
				flag ? op_plus : op_minus);

	}else{
		switch(curtok){
			case token_andsc:
				/* GNU &&label */
				EAT(curtok);
				e = expr_new_addr();
				e->spel = token_current_spel();
				EAT(token_identifier);
				break;

			case token_and:
				e = expr_new_addr();
				goto do_parse;

			case token_multiply:
				EAT(curtok);
				e = expr_new_deref(parse_expr_cast());
				break;

			case token_plus:
			case token_minus:
			case token_bnot:
			case token_not:
				e = expr_new_op(curtok_to_op());
do_parse:
				EAT(curtok);
				e->lhs = parse_expr_cast();
				break;

			case token_sizeof:
				EAT(token_sizeof);
				e = parse_expr_sizeof_typeof(0);
				break;

			default:
				e = parse_expr_postfix();
		}
	}

	return e;
}

expr *parse_expr_generic(expr *(*above)(), enum token t, ...)
{
	expr *e = above();
	va_list l;

	va_start(l, t);
	while(curtok == t || curtok_in_list(l)){
		expr *join = expr_new_op(curtok_to_op());
		EAT(curtok);
		join->lhs = e;
		join->rhs = above();
		e = join;
	}

	va_end(l);
	return e;
}

#define PARSE_DEFINE(this, above, ...) \
expr *parse_expr_ ## this()            \
{                                      \
	return parse_expr_generic(           \
			parse_expr_ ## above,            \
			__VA_ARGS__, token_unknown);     \
}

/* cast is handled in unary instead */
PARSE_DEFINE(multi,     /*cast,*/unary,  token_multiply, token_divide, token_modulus)
PARSE_DEFINE(additive,    multi,         token_plus, token_minus)
PARSE_DEFINE(shift,       additive,      token_shiftl, token_shiftr)
PARSE_DEFINE(relational,  shift,         token_lt, token_gt, token_le, token_ge)
PARSE_DEFINE(equality,    relational,    token_eq, token_ne)
PARSE_DEFINE(binary_and,  equality,      token_and)
PARSE_DEFINE(binary_xor,  binary_and,    token_xor)
PARSE_DEFINE(binary_or,   binary_xor,    token_or)
PARSE_DEFINE(logical_and, binary_or,     token_andsc)
PARSE_DEFINE(logical_or,  logical_and,   token_orsc)

expr *parse_expr_conditional()
{
	expr *e = parse_expr_logical_or();

	if(accept(token_question)){
		expr *q = expr_new_if(e);

		if(accept(token_colon)){
			q->lhs = NULL; /* sentinel */
		}else{
			q->lhs = parse_expr_exp();
			EAT(token_colon);
		}
		q->rhs = parse_expr_conditional();

		return q;
	}
	return e;
}

expr *parse_expr_assignment()
{
	expr *e;

	e = parse_expr_conditional();

	if(accept(token_assign)){
		return expr_new_assign(e, parse_expr_assignment());

	}else if(curtok_is_compound_assignment()){
		/* +=, ... - only evaluate the lhs once*/
		enum op_type op = curtok_to_compound_op();

		EAT(curtok);

		return expr_new_assign_compound(e, parse_expr_assignment(), op);
	}

	return e;
}

expr *parse_expr_exp()
{
	expr *e;

	e = parse_expr_assignment();

	if(accept(token_comma)){
		expr *ret = expr_new_comma();
		ret->lhs = e;
		ret->rhs = parse_expr_exp();
		return ret;
	}
	return e;
}

expr **parse_funcargs()
{
	expr **args = NULL;

	while(curtok != token_close_paren){
		expr *arg = parse_expr_no_comma();
		if(!arg)
			DIE_AT(&arg->where, "expected: funcall arg");
		dynarray_add((void ***)&args, arg);

		if(curtok == token_close_paren)
			break;
		EAT(token_comma);
	}

	return args;
}

void parse_test_init_expr(stmt *t)
{
	decl **c99_ucc_inits;

	EAT(token_open_paren);

	c99_ucc_inits = parse_decls_one_type();
	if(c99_ucc_inits){
		t->flow = stmt_flow_new(symtab_new(t->symtab));

		current_scope = t->flow->for_init_symtab;

		t->flow->for_init_decls = c99_ucc_inits;
	}else{
		t->expr = parse_expr_exp();
	}

	EAT(token_close_paren);
}

stmt *parse_if()
{
	stmt *t = STAT_NEW(if);

	EAT(token_if);

	parse_test_init_expr(t);

	t->lhs = parse_stmt();

	if(accept(token_else))
		t->rhs = parse_stmt();

	if(t->flow)
		current_scope = current_scope->parent;

	return t;
}

stmt *parse_switch()
{
	stmt *t = STAT_NEW(switch);
	stmt *old = current_break_target;
	stmt *old_sw = current_switch;

	current_switch = current_break_target = t;

	EAT(token_switch);

	parse_test_init_expr(t);

	t->lhs = parse_stmt();

	current_break_target = old;
	current_switch = old_sw;

	return t;
}

stmt *parse_do()
{
	stmt *t = STAT_NEW(do);

	current_continue_target = current_break_target = t;

	EAT(token_do);

	t->lhs = parse_stmt();

	EAT(token_while);
	EAT(token_open_paren);
	t->expr = parse_expr_exp();
	EAT(token_close_paren);
	EAT(token_semicolon);

	return t;
}

stmt *parse_while()
{
	stmt *t = STAT_NEW(while);

	current_continue_target = current_break_target = t;

	EAT(token_while);

	parse_test_init_expr(t);

	t->lhs = parse_stmt();

	return t;
}

stmt *parse_for()
{
	stmt *s = STAT_NEW(for);
	stmt_flow *sf;

	current_continue_target = current_break_target = s;

	EAT(token_for);
	EAT(token_open_paren);

	sf = s->flow = stmt_flow_new(symtab_new(s->symtab));

	current_scope = sf->for_init_symtab;

#define SEMI_WRAP(code)         \
	if(!accept(token_semicolon)){ \
		code;                       \
		EAT(token_semicolon);       \
	}

	SEMI_WRAP(
			decl **c99inits = parse_decls_one_type();
			if(c99inits)
				sf->for_init_decls = c99inits;
			else
				sf->for_init = parse_expr_exp()
	);

	SEMI_WRAP(sf->for_while = parse_expr_exp());

#undef SEMI_WRAP

	if(!accept(token_close_paren)){
		sf->for_inc   = parse_expr_exp();
		EAT(token_close_paren);
	}

	s->lhs = parse_stmt();

	current_scope = current_scope->parent;

	return s;
}

void parse_static_assert(void)
{
	while(accept(token__Static_assert)){
		static_assert *sa = umalloc(sizeof *sa);

		sa->scope = current_scope;

		EAT(token_open_paren);
		sa->e = parse_expr_no_comma();
		EAT(token_comma);

		token_get_current_str(&sa->s, NULL);

		EAT(token_string);
		EAT(token_close_paren);
		EAT(token_semicolon);

		dynarray_add((void ***)&static_asserts, sa);
	}
}

asm_inout **parse_asm_inout()
{
	asm_inout **inouts;

	if(curtok != token_string)
		return NULL;

	inouts = NULL;

	do{
		asm_inout *io = umalloc(sizeof *io);

		if(accept(token_open_square)){
			/* : [name] "constraints"(expr) */
			ICE("TODO: [] named constraint expression");

			EAT(token_close_square);
		}

		token_get_current_str(&io->constraints, NULL);
		EAT(token_string);

		EAT(token_open_paren);
		io->exp = parse_expr_exp();
		EAT(token_close_paren);

		dynarray_add((void ***)&inouts, io);
	}while(accept(token_comma));

	return inouts;
}

char **parse_asm_clobbers(void)
{
	char **ret = NULL;

	while(curtok == token_string){
		char *s;

		token_get_current_str(&s, NULL);
		EAT(token_string);

		dynarray_add((void ***)&ret, s);
	}

	return ret;
}

stmt *parse_asm(void)
{
	stmt *asm_st;
	asm_args *bits;

	asm_st = STAT_NEW(asm);
	bits = asm_st->asm_bits = umalloc(sizeof *asm_st->asm_bits);

	EAT(token_asm);
	EAT(token_open_paren);

	token_get_current_str(&bits->cmd, NULL);
	EAT(token_string);

	/* input operands */
	if((bits->extended = accept(token_colon))){
		bits->outputs = parse_asm_inout();

		/* output operands */
		if(accept(token_colon))
			bits->inputs = parse_asm_inout();

		/* clobber list */
		if(accept(token_colon))
			bits->clobbers = parse_asm_clobbers();
	}

	EAT(token_close_paren);
	EAT(token_semicolon);

	return asm_st;
}

void parse_got_decls(decl **decls, stmt *codes_init)
{
	dynarray_add_array((void ***)&codes_init->decls, (void **)decls);
}

stmt *parse_stmt_and_decls()
{
	stmt *codes = STAT_NEW_NEST(code);
	int last;

	current_scope = codes->symtab;

	last = 0;
	do{
		decl **decls;
		stmt *sub;

		parse_static_assert();

		decls = parse_decls_multi_type(DECL_MULTI_ACCEPT_FUNC_DECL);
		sub = NULL;

		if(decls){
			/*
			 * create an implicit block for the decl i.e.
			 *
			 * int i;              int i;
			 * i = 5;              i = 5;
			 *                     {
			 * int j;   ---->        int j;
			 * j = 2;                j = 2;
			 *                     }
			 */

			/* optimise - initial-block-decls doesn't need a sub-block */
			if(!codes->codes){
				parse_got_decls(decls, codes);
				goto normal;
			}else{
				static int warned = 0;
				if(!warned){
					warned = 1;
					cc1_warn_at(&decls[0]->where, 0, 1, WARN_MIXED_CODE_DECLS,
							"mixed code and declarations");
				}
			}

			/* new sub block */
			if(curtok != token_close_block)
				sub = parse_stmt_and_decls();

			if(!sub){
				/* decls aren't useless - { int i = f(); } - f called */
				sub = STAT_NEW(code);
			}

			/* mark as internal - for duplicate checks */
			sub->symtab->internal_nest = 1;

			parse_got_decls(decls, sub);

			last = 1;
		}else{
normal:
			if(curtok != token_close_block){
				/* fine with a normal statement */
				sub = parse_stmt();
			}else{
				last = 1;
			}
		}

		if(decls)
			dynarray_free((void ***)&decls, NULL);

		if(sub)
			dynarray_add((void ***)&codes->codes, sub);
	}while(!last);


	current_scope = codes->symtab->parent;

	return codes;
}

#ifdef SYMTAB_DEBUG
static int print_indent = 0;

#define INDENT(...) indent(), fprintf(stderr, __VA_ARGS__)

void indent()
{
	for(int c = print_indent; c; c--)
		fputc('\t', stderr);
}

void print_stmt_and_decls(stmt *t)
{
	INDENT("decls: (symtab %p, parent %p)\n", t->symtab, t->symtab->parent);

	print_indent++;
	for(decl **i = t->decls; i && *i; i++)
		INDENT("%s\n", (*i)->spel);
	print_indent--;

	if(!t->decls)
		print_indent++, INDENT("NONE\n"), print_indent--;

	INDENT("codes:\n");
	for(stmt **i = t->codes; i && *i; i++){
		stmt *s = *i;
		if(stmt_kind(s, code)){
			print_indent++;
			INDENT("more-codes:\n");
			print_indent++;
			print_stmt_and_decls(s);
			print_indent -= 2;
		}else{
			print_indent++;
			INDENT("%s%s%s (symtab %p)\n",
					s->f_str(),
					stmt_kind(s, expr) ? ": " : "",
					stmt_kind(s, expr) ? s->expr->f_str() : "",
					s->symtab);
			print_indent--;
		}
	}
	if(!t->codes)
		print_indent++, INDENT("NONE\n"), print_indent--;
}
#endif

stmt *parse_stmt_block()
{
	stmt *t;

	EAT(token_open_block);

	t = parse_stmt_and_decls();

	EAT(token_close_block);

#ifdef SYMTAB_DEBUG
	fprintf(stderr, "Parsed statement block:\n");
	print_stmt_and_decls(t);
#endif

	return t;
}

stmt *parse_label_next(stmt *lbl)
{
	lbl->lhs = parse_stmt();
	/*
	 * a label must have a block of code after it:
	 *
	 * if(x)
	 *   lbl:
	 *   printf("yo\n");
	 *
	 * both the label and the printf statements are in the if
	 * as a compound statement
	 */
	return lbl;
}

stmt *parse_stmt()
{
	stmt *t;

	switch(curtok){
		case token_semicolon:
			t = STAT_NEW(noop);
			EAT(token_semicolon);
			return t;

		case token_break:
		case token_continue:
		case token_goto:
		case token_return:
		{
			if(accept(token_break)){
				t = STAT_NEW(break);
				t->parent = current_break_target;

			}else if(accept(token_continue)){
				t = STAT_NEW(continue);
				t->parent = current_continue_target;

			}else if(accept(token_return)){
				t = STAT_NEW(return);

				if(curtok != token_semicolon)
					t->expr = parse_expr_exp();
			}else{
				EAT(token_goto);

				t = STAT_NEW(goto);

				if(accept(token_multiply)){
					/* computed goto */
					t->expr = parse_expr_exp();
					t->expr->expr_computed_goto = 1;
				}else{
					t->expr = parse_expr_identifier();
				}
			}
			EAT(token_semicolon);
			return t;
		}

		case token_if:
			return parse_if();

		{
			stmt *(*parse_f)();
			stmt *ret, *old[2];

		case token_while:
			parse_f = parse_while;
			goto flow;
		case token_do:
			parse_f = parse_do;
			goto flow;
		case token_for:
			parse_f = parse_for;
			goto flow;

flow:
			old[0] = current_continue_target;
			old[1] = current_break_target;

			ret = parse_f();

			current_continue_target = old[0];
			current_break_target    = old[1];

			return ret;
		}

		case token_open_block: return parse_stmt_block();

		case token_switch:
			return parse_switch();

		case token_asm:
			return parse_asm();

		case token_default:
			EAT(token_default);
			EAT(token_colon);
			t = STAT_NEW(default);
			t->parent = current_switch;
			return parse_label_next(t);
		case token_case:
		{
			expr *a;
			EAT(token_case);
			a = parse_expr_exp();
			if(accept(token_elipsis)){
				t = STAT_NEW(case_range);
				t->parent = current_switch;
				t->expr  = a;
				t->expr2 = parse_expr_exp();
			}else{
				t = STAT_NEW(case);
				t->expr = a;
				t->parent = current_switch;
			}

			EAT(token_colon);
			return parse_label_next(t);
		}

		default:
			t = expr_to_stmt(parse_expr_exp(), current_scope);

			if(expr_kind(t->expr, identifier) && accept(token_colon)){
				stmt_mutate_wrapper(t, label);
				return parse_label_next(t);
			}else{
				EAT(token_semicolon);
				return t;
			}
	}

	/* unreachable */
}

symtable *parse()
{
	symtable *globals;
	decl **decls = NULL;
	int i;
	int warned = 0;

	current_scope = globals = symtab_new(NULL);

	for(;;){
		decl **new = parse_decls_multi_type(DECL_MULTI_CAN_DEFAULT | DECL_MULTI_ACCEPT_FUNC_CODE);

		if(new){
			dynarray_add_array((void ***)&decls, (void **)new);
			free(new);
		}

		if(accept(token_semicolon)){
			if(!warned){
				WARN_AT(NULL, "extra semi-colon after global decl");
				warned = 1;
			}
			continue;
		}
		break;
	}

	EAT(token_eof);

	if(parse_had_error)
		exit(1);

	if(decls)
		for(i = 0; decls[i]; i++)
			symtab_add(globals, decls[i], sym_global, SYMTAB_NO_SYM, SYMTAB_APPEND);

	globals->static_asserts = static_asserts;

	return globals;
}
