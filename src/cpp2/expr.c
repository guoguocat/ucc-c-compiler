#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "expr.h"
#include "expr_tok.h"

#include "main.h"
#include "../util/alloc.h"
#include "../util/util.h"

typedef signed char e_op;
struct expr
{
	enum
	{
		E_IDENT, E_NUM, E_OP
	} type;

	union
	{
		expr_n num;
		struct
		{
			e_op op;
			expr *lhs, *rhs;
		} op;
	} bits;
};

static int is_op(e_op op)
{
	switch(op){
		case tok_multiply: case tok_divide: case tok_modulus:
		case tok_plus:     case tok_minus:  case tok_xor:
		case tok_or:       case tok_and:    case tok_orsc:
		case tok_andsc:    case tok_shiftl: case tok_shiftr:
		case tok_not:      case tok_bnot:   case tok_eq:
		case tok_ne:       case tok_le:     case tok_lt:
		case tok_ge:       case tok_gt:
				return 1;
	}
	return 0;
}

static expr *expr_new(int ty)
{
	expr *e = umalloc(sizeof *e);
	e->type = ty;
	return e;
}

static expr *expr_ident(void)
{
	expr *e = expr_new(E_IDENT);
	return e;
}

static expr *expr_num(expr_n num)
{
	expr *e = expr_new(E_NUM);
	e->bits.num = num;
	return e;
}

static expr *expr_op(e_op op, expr *l, expr *r)
{
	expr *e = expr_new(E_OP);
	e->bits.op.lhs = l;
	e->bits.op.rhs = r;
	e->bits.op.op  = op;
	return e;
}

static expr *parse_primary(void)
{
	switch(tok_cur){
		case tok_ident: tok_next(); return expr_ident();
		case tok_num:   tok_next(); return expr_num(tok_cur_num);
		default:
			break;
	}

	CPP_DIE("expression expected (got %c)", tok_cur);
}

static int preds[255];
#define PRECEDENCE(x) preds[x - MIN_OP]
#define MAX_PRI 20

static void expr_init(void)
{
	static int init = 0;
	unsigned i;

	if(init)
		return;
	init = 1;

	for(i = 0; i < sizeof(preds)/sizeof(*preds); i++)
		preds[i] = MAX_PRI; /* any large value */

#define PRED_SET(op, n) PRECEDENCE(tok_ ## op) = n
	/* smaller = tighter binding */
	PRED_SET(multiply,   1);
	PRED_SET(divide,     1);
	PRED_SET(modulus,    1);

	PRED_SET(plus,       2);
	PRED_SET(minus,      2);

	PRED_SET(shiftl,     3);
	PRED_SET(shiftr,     3);

	PRED_SET(le,         4);
	PRED_SET(lt,         4);
	PRED_SET(ge,         4);
	PRED_SET(gt,         4);

	PRED_SET(eq,         5);
	PRED_SET(ne,         5);

	PRED_SET(and,        6);
	PRED_SET(xor,        7);
	PRED_SET(or,         8);

	PRED_SET(andsc,      9);
	PRED_SET(orsc,      10);
#undef PRED_SET
}

static expr *parse_rhs(expr *lhs, int priority)
{
	for(;;){
    int this_pri, next_pri;
		e_op op;
		expr *rhs;

		if(tok_cur == tok_eof)
			return lhs;

		op = tok_cur;
		if(!is_op(op))
			CPP_DIE("operator expected, got '%c'", op);

		this_pri = PRECEDENCE(op);
    if(this_pri > priority)
      return lhs;

		/* eat the op */
		tok_next();

		rhs = parse_primary();

		/* now on the next op, or eof (in which case, precedence returns -1 */
    next_pri = PRECEDENCE(tok_cur);
    if(next_pri < this_pri) {
			/* next is tighter, give it our rhs as its lhs */
      rhs = parse_rhs(rhs, next_pri);
		}

    lhs = expr_op(op, lhs, rhs);
  }
}

static expr *parse(void)
{
  return parse_rhs(parse_primary(), MAX_PRI);
}

expr *expr_parse(char *str)
{
	expr *e;

	expr_init();

	tok_pos = str;
	tok_next();

	e = parse();

	if(tok_cur != tok_eof)
		CPP_DIE("'%s' at end of expression", tok_pos);

	return e;
}

/* eval */
expr_n expr_eval(expr *e_)
{
	const expr ke = *e_;
	free(e_);

	switch(ke.type){
		case E_IDENT:
			return 0; /* identifiers are zero */
		case E_NUM:
			return ke.bits.num;
		case E_OP:
		{
			expr_n nums[2];

			nums[0] = expr_eval(ke.bits.op.lhs);
			nums[1] = expr_eval(ke.bits.op.rhs);

			switch(ke.bits.op.op){
				case '*':
				case '/':
					if(nums[1] == 0)
						CPP_DIE("%s by zero",
								ke.bits.op.op == '/' ? "division" : "modulo");
				default:
					break;
			}

			switch(ke.bits.op.op){
#define OP(ty, op) case tok_ ## ty: return nums[0] op nums[1]
				OP(divide, /);
				OP(modulus, %);
				OP(multiply, *);
				OP(plus, +);
				OP(minus, -);
				OP(xor, ^);
				OP(or, |);
				OP(and, &);
				OP(orsc, ||);
				OP(andsc, &&);
				OP(shiftl, <<);
				OP(shiftr, >>);
				OP(eq, ==);
				OP(ne, !=);
				OP(le, <=);
				OP(lt, <);
				OP(ge, >=);
				OP(gt, >);
#undef OP

				case tok_not:
				case tok_bnot:
					ICE("TODO: unary");
			}
			ICE("bad op");
		}
	}

	ICE("bad expr type");
}