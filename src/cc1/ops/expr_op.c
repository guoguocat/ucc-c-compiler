#include <string.h>
#include <stdlib.h>
#include "ops.h"
#include "../sue.h"
#include "expr_op.h"

const char *str_expr_op()
{
	return "op";
}

static void operate(
		intval *lval, intval *rval,
		enum op_type op,
		intval *piv, enum constyness *pconst_type,
		where *where)
{
#define OP(a, b) case a: piv->val = lval->val b rval->val; return
	switch(op){
		OP(op_multiply,   *);
		OP(op_eq,         ==);
		OP(op_ne,         !=);
		OP(op_le,         <=);
		OP(op_lt,         <);
		OP(op_ge,         >=);
		OP(op_gt,         >);
		OP(op_xor,        ^);
		OP(op_or,         |);
		OP(op_and,        &);
		OP(op_orsc,       ||);
		OP(op_andsc,      &&);
		OP(op_shiftl,     <<);
		OP(op_shiftr,     >>);

		case op_modulus:
		case op_divide:
		{
			long l = lval->val, r = rval->val;

			if(r){
				piv->val = op == op_divide ? l / r : l % r;
				return;
			}
			warn_at(where, 1, "division by zero");
			*pconst_type = CONST_NO;
			return;
		}

		case op_plus:
			piv->val = lval->val + (rval ? rval->val : 0);
			return;

		case op_minus:
			piv->val = rval ? lval->val - rval->val : -lval->val;
			return;

		case op_not:  piv->val = !lval->val; return;
		case op_bnot: piv->val = ~lval->val; return;

		case op_deref:
			/*
			 * potential for major ICE here
			 * I mean, we might not even be dereferencing the right size pointer
			 */
			/*
			switch(lhs->vartype->primitive){
				case type_int:  return *(int *)lhs->val.s;
				case type_char: return *       lhs->val.s;
				default:
					break;
			}

			ignore for now, just deal with simple stuff
			*/
			/*if(lhs->ptr_safe && expr_kind(lhs, addr)){
				if(lhs->array_store->type == array_str){
					/piv->val = lhs->val.exprs[0]->val.i;/
					piv->val = *lhs->val.s;
					return;
				}
			}*/
			*pconst_type = CONST_NO;
			return;

		case op_struct_ptr:
		case op_struct_dot:
			*pconst_type = CONST_NO;
			return;

		case op_unknown:
			break;
	}

	ICE("unhandled type");
}

void operate_optimise(expr *e)
{
	/* TODO */

	switch(e->op){
		case op_orsc:
		case op_andsc:
			/* check if one side is (&& ? false : true) and short circuit it without needing to check the other side */
			if(expr_kind(e->lhs, val) || expr_kind(e->rhs, val))
				POSSIBLE_OPT(e, "short circuit const");
			break;

#define VAL(e, x) (expr_kind(e, val) && e->bits.iv.val == x)

		case op_plus:
		case op_minus:
			if(VAL(e->lhs, 0) || (e->rhs ? VAL(e->rhs, 0) : 0))
				POSSIBLE_OPT(e, "zero being added or subtracted");
			break;

		case op_multiply:
			if(VAL(e->lhs, 1) || VAL(e->lhs, 0) || (e->rhs ? VAL(e->rhs, 1) || VAL(e->rhs, 0) : 0))
				POSSIBLE_OPT(e, "1 or 0 being multiplied");
			else
		case op_divide:
			if(VAL(e->rhs, 1))
				POSSIBLE_OPT(e, "divide by 1");
			break;

		default:
			break;
#undef VAL
	}
}

void fold_const_expr_op(expr *e, intval *piv, enum constyness *pconst_type)
{
	intval lhs, rhs;
	enum constyness l_const, r_const;

	const_fold(e->lhs, &lhs, &l_const);
	if(e->rhs){
		const_fold(e->rhs, &rhs, &r_const);
	}else{
		r_const = CONST_WITH_VAL;
	}

	if(l_const == CONST_WITH_VAL && r_const == CONST_WITH_VAL){
		*pconst_type = CONST_WITH_VAL;
		operate(&lhs, e->rhs ? &rhs : NULL, e->op, piv, pconst_type, &e->where);
	}else{
		*pconst_type = CONST_WITHOUT_VAL;
		operate_optimise(e);
	}
}

void fold_op_struct(expr *e, symtable *stab)
{
	/*
	 * lhs = any ptr-to-struct expr
	 * rhs = struct member ident
	 */
	const int ptr_depth_exp = e->op == op_struct_ptr ? 1 : 0;
	struct_union_enum_st *sue;
	char *spel;

	fold_expr(e->lhs, stab);
	/* don't fold the rhs - just a member name */

	if(!expr_kind(e->rhs, identifier))
		DIE_AT(&e->rhs->where, "struct member must be an identifier (got %s)", e->rhs->f_str());
	spel = e->rhs->spel;

	/* we access a struct, of the right ptr depth */
	if(!decl_is_struct_or_union_possible_ptr(e->lhs->tree_type)
	|| decl_ptr_depth(e->lhs->tree_type) != ptr_depth_exp)
	{
		const int ident = expr_kind(e->lhs, identifier);

		DIE_AT(&e->lhs->where, "%s%s%s is not a %sstruct or union (member %s)",
				decl_to_str(e->lhs->tree_type),
				ident ? " " : "",
				ident ? e->lhs->spel : "",
				ptr_depth_exp == 1 ? "pointer-to-" : "",
				spel);
	}

	sue = e->lhs->tree_type->type->sue;

	if(sue_incomplete(sue))
		DIE_AT(&e->lhs->where, "%s incomplete type (%s)",
				ptr_depth_exp == 1
				? "dereferencing pointer to"
				: "use of",
				type_to_str(e->lhs->tree_type->type));

	/* found the struct, find the member */
	e->rhs->tree_type = struct_union_member_find(sue, spel, &e->where);

	/*
	 * if it's a.b, convert to (&a)->b for asm gen
	 * e = { lhs = "a", rhs = "b", type = dot }
	 * e = { lhs = { expr = "a", type = addr }, rhs = "b", type = ptr }
	 */
	if(ptr_depth_exp == 0){
		expr *new = expr_new_wrapper(addr);

		new->lhs = e->lhs;
		e->lhs = new;

		expr_mutate_wrapper(e, op);
		e->op   = op_struct_ptr;

		fold_expr(e->lhs, stab);
	}

	e->tree_type = decl_copy(e->rhs->tree_type);
	/* pull qualifiers from the struct to the member */
	e->tree_type->type->qual |= e->lhs->tree_type->type->qual;
}

void fold_deref(expr *e)
{
	if(decl_attr_present(op_deref_expr(e)->tree_type->attr, attr_noderef))
		WARN_AT(&op_deref_expr(e)->where, "dereference of noderef expression");

	/* check for *&x */
	if(expr_kind(op_deref_expr(e), addr))
		WARN_AT(&op_deref_expr(e)->where, "possible optimisation for *& expression");

	e->tree_type = decl_ptr_depth_dec(decl_copy(op_deref_expr(e)->tree_type), &e->where);
}

void fold_expr_op(expr *e, symtable *stab)
{
#define IS_PTR(x) decl_ptr_depth(x->tree_type)

#define SPEL_IF_IDENT(hs)                            \
					expr_kind(hs, identifier) ? " ("     : "", \
					expr_kind(hs, identifier) ? hs->spel : "", \
					expr_kind(hs, identifier) ? ")"      : ""  \

#define RHS e->rhs
#define LHS e->lhs

	UCC_ASSERT(e->op != op_unknown, "unknown op in expression at %s",
			where_str(&e->where));

	if(e->op == op_struct_ptr || e->op == op_struct_dot){
		fold_op_struct(e, stab);
		return;
	}

	fold_expr(e->lhs, stab);
	fold_disallow_st_un(e->lhs, "op-lhs");

	if(e->rhs){
		char bufa[DECL_STATIC_BUFSIZ], bufb[DECL_STATIC_BUFSIZ];

		fold_expr(e->rhs, stab);
		fold_disallow_st_un(e->rhs, "op-rhs");

		if(decl_is_void(e->lhs->tree_type) || decl_is_void(e->rhs->tree_type))
			DIE_AT(&e->where, "use of void expression");

		fold_decl_equal(e->lhs->tree_type, e->rhs->tree_type,
				&e->where, WARN_COMPARE_MISMATCH,
				"mismatching types in %s (%s and %s)",
				op_to_str(e->op),
				decl_to_str_r(bufa, e->lhs->tree_type),
				decl_to_str_r(bufb, e->rhs->tree_type));

		if(op_is_cmp(e->op) && e->lhs->tree_type->type->is_signed != e->rhs->tree_type->type->is_signed){

			/*
			 * assert(LHS == UNSIGNED);
			 * vals default to signed, change to unsigned
			 */

			if(expr_kind(RHS, val) && RHS->bits.iv.val >= 0){
				UCC_ASSERT(!e->lhs->tree_type->type->is_signed, "signed-unsigned assumption failure");
				RHS->tree_type->type->is_signed = 0;
			}else if(expr_kind(LHS, val) && LHS->bits.iv.val >= 0){
				UCC_ASSERT(!e->rhs->tree_type->type->is_signed, "signed-unsigned assumption failure");
				LHS->tree_type->type->is_signed = 0;
			}else{
					cc1_warn_at(&e->where, 0, 1, WARN_SIGN_COMPARE, "comparison between signed and unsigned%s%s%s%s%s%s",
							SPEL_IF_IDENT(LHS), SPEL_IF_IDENT(RHS));
			}
		}
	}

	if(e->op == op_deref){
		fold_deref(e);
	}else{
		/*
		 * look either side - if either is a pointer, take that as the tree_type
		 *
		 * operation between two values of any type
		 *
		 * TODO: checks for pointer + pointer (invalid), etc etc
		 */

		if(e->rhs){
			if(IS_PTR(e->rhs)){
				e->tree_type = decl_copy(e->rhs->tree_type);
			}else{
				goto norm_tt;
			}
		}else{
norm_tt:
			/* if we have a _comparison_ (e.g. between enums), convert to int */

			if(op_is_cmp(e->op)){
				e->tree_type = decl_new();
				e->tree_type->type->primitive = type_int;
			}else{
				e->tree_type = decl_copy(e->lhs->tree_type);
			}
		}
	}

	if(e->rhs){
		/* need to do this check _after_ we get the correct tree type */
		if((e->op == op_plus || e->op == op_minus)
		&& decl_ptr_depth(e->tree_type)
		&& !e->op_no_ptr_mul)
		{
			/* 2 + (void *)5 is 7, not 2 + 8*5 */
			if(decl_is_void_ptr(e->tree_type)){
				cc1_warn_at(&e->tree_type->type->where, 0, 1, WARN_VOID_ARITH, "arithmetic with void pointer");
			}else{
				/*
				 * subtracting two pointers - need to divide by sizeof(typeof(*expr))
				 * adding int to a pointer - need to multiply by sizeof(typeof(*expr))
				 */

				if(e->op == op_minus){
					/* need to apply the divide to the current 'e' */
					expr *sub = expr_new_op(e->op);
					decl *const cpy = decl_ptr_depth_dec(decl_copy(e->tree_type), &e->where);

					memcpy(&sub->where, &e->where, sizeof sub->where);

					sub->lhs = e->lhs;
					sub->rhs = e->rhs;

					sub->tree_type = e->tree_type;
					e->tree_type = decl_new();
					e->tree_type->type->primitive = type_int;

					e->op = op_divide;

					e->lhs = sub;
					e->rhs = expr_new_val(decl_size(cpy));
					fold_expr(e->rhs, stab);

					decl_free(cpy);

				}else{
					if(decl_ptr_depth(e->lhs->tree_type))
						/* lhs is the pointer, we're adding on rhs, hence multiply rhs by lhs's ptr size */
						e->rhs = expr_ptr_multiply(e->rhs, e->lhs->tree_type);
					else
						e->lhs = expr_ptr_multiply(e->lhs, e->rhs->tree_type);
				}
			}
		}

		if(e->op == op_minus && IS_PTR(e->lhs) && IS_PTR(e->rhs)){
			/* ptr - ptr = int */
			/* FIXME: ptrdiff_t */
			e->tree_type->type->primitive = type_int;
		}
	}

#undef SPEL_IF_IDENT
#undef IS_PTR
}

void gen_expr_str_op(expr *e, symtable *stab)
{
	(void)stab;
	idt_printf("op: %s\n", op_to_str(e->op));
	gen_str_indent++;

	if(e->op == op_deref){
		idt_printf("deref size: %s ", decl_to_str(e->tree_type));
		fputc('\n', cc1_out);
	}

#define PRINT_IF(hs) if(e->hs) print_expr(e->hs)
	PRINT_IF(lhs);
	PRINT_IF(rhs);
#undef PRINT_IF

	gen_str_indent--;
}

static void asm_idiv(expr *e, symtable *tab)
{
	/*
	 * idiv — Integer Division
	 * The idiv asm_temp divides the contents of the 64 bit integer EDX:EAX (constructed by viewing EDX as the most significant four bytes and EAX as the least significant four bytes) by the specified operand value. The quotient result of the division is stored into EAX, while the remainder is placed in EDX.
	 * Syntax
	 * idiv <reg32>
	 * idiv <mem>
	 *
	 * Examples
	 *
	 * idiv ebx — divide the contents of EDX:EAX by the contents of EBX. Place the quotient in EAX and the remainder in EDX.
	 * idiv DWORD PTR [var] — divide the contents of EDX:EAS by the 32-bit value stored at memory location var. Place the quotient in EAX and the remainder in EDX.
	 */

	gen_expr(e->lhs, tab);
	gen_expr(e->rhs, tab);
	/* pop top stack (rhs) into b, and next top into a */

#define IDIV_SIGN_EXTEND

#ifndef  IDIV_SIGN_EXTEND
# warning old broken division
	asm_temp(1, "xor rdx,rdx");
#endif

	asm_temp(1, "pop rbx");
	asm_temp(1, "pop rax");
#ifdef IDIV_SIGN_EXTEND
	asm_temp(1, "cqo ; rax -> rdx:rax"); /* convert quad to oct - cqto in AT&T */
#endif
	asm_temp(1, "idiv rbx");

	asm_temp(1, "push r%cx", e->op == op_divide ? 'a' : 'd');
}

static void asm_compare(expr *e, symtable *tab)
{
	const char *cmp = NULL;

	gen_expr(e->lhs, tab);
	gen_expr(e->rhs, tab);
	asm_temp(1, "pop rbx");
	asm_temp(1, "pop rcx");
	asm_temp(1, "xor rax,rax"); /* must be before cmp */
	asm_temp(1, "cmp rcx,rbx");

	/* check for unsigned, since signed isn't explicitly set */
#define SIGNED(s, u) e->tree_type->type->is_signed ? s : u

	switch(e->op){
		case op_eq: cmp = "e";  break;
		case op_ne: cmp = "ne"; break;

		case op_le: cmp = SIGNED("le", "be"); break;
		case op_lt: cmp = SIGNED("l",  "b");  break;
		case op_ge: cmp = SIGNED("ge", "ae"); break;
		case op_gt: cmp = SIGNED("g",  "a");  break;

		default:
			ICE("%s: unhandled comparison", __func__);
	}

	asm_temp(1, "set%s al", cmp);
	asm_temp(1, "push rax");
}

static void asm_shortcircuit(expr *e, symtable *tab)
{
	char *baillabel = asm_label_code("shortcircuit_bail");
	gen_expr(e->lhs, tab);

	asm_temp(1, "mov rax,[rsp]");
	asm_temp(1, "test rax,rax");
	/* leave the result on the stack (if false) and bail */
	asm_temp(1, "j%sz %s", e->op == op_andsc ? "" : "n", baillabel);
	asm_temp(1, "pop rax");
	gen_expr(e->rhs, tab);

	/* must convert to 1 or 0 */
	asm_temp(1, "pop rcx");
	asm_temp(1, "xor rax, rax");
	asm_temp(1, "test rcx, rcx");
	asm_temp(1, "setnz al");
	asm_temp(1, "push rax");

	asm_label(baillabel);

	free(baillabel);
}

void asm_operate_struct(expr *e, symtable *tab)
{
	(void)tab;

	UCC_ASSERT(e->op == op_struct_ptr, "a.b should have been handled by now");

	gen_expr(e->lhs, tab);

	/* pointer to the struct is on the stack, get from the offset */
	asm_temp(1, "pop rax ; struct ptr");
	asm_temp(1, "add rax, %d ; offset of member %s",
			e->rhs->tree_type->struct_offset,
			e->rhs->spel);

	if(decl_is_array(e->rhs->tree_type))
		asm_temp(1, "; array - got address");
	else
		asm_indir(ASM_INDIR_GET, e->tree_type, 'a', 'a', "val from struct");

	asm_temp(1, "push rax");
}

void gen_expr_op(expr *e, symtable *tab)
{
	const char *instruct = NULL;
	const char *rhs = "rcx";

	switch(e->op){
		/* normal mafs */
		case op_multiply: instruct = "imul"; break;
		case op_plus:     instruct = "add";  break;
		case op_xor:      instruct = "xor";  break;
		case op_or:       instruct = "or";   break;
		case op_and:      instruct = "and";  break;

		/* single register op */
		case op_minus: instruct = e->rhs ? "sub" : "neg"; break;
		case op_bnot:  instruct = "not";                  break;

#define SHIFT(side) \
		case op_shift ## side: instruct = "sh" # side; rhs = "cl"; break

		SHIFT(l);
		SHIFT(r);

		case op_not:
			/* compare with 0 */
			gen_expr(e->lhs, tab);
			asm_temp(1, "xor rbx,rbx");
			asm_temp(1, "pop rax");
			asm_temp(1, "test rax,rax");
			asm_temp(1, "setz bl");
			asm_temp(1, "push rbx");
			return;

		case op_deref:
			gen_expr(e->lhs, tab);
			asm_temp(1, "pop rax");

			asm_indir(ASM_INDIR_GET, e->tree_type, 'a', 'a', NULL);

			asm_temp(1, "push rax");
			return;

		case op_struct_dot:
		case op_struct_ptr:
			asm_operate_struct(e, tab);
			return;

		/* comparison */
		case op_eq:
		case op_ne:
		case op_le:
		case op_lt:
		case op_ge:
		case op_gt:
			asm_compare(e, tab);
			return;

		/* shortcircuit */
		case op_orsc:
		case op_andsc:
			asm_shortcircuit(e, tab);
			return;

		case op_divide:
		case op_modulus:
			asm_idiv(e, tab);
			return;

		case op_unknown:
			ICE("asm_operate: unknown operator got through");
	}

	/* asm_temp(1, "%s rax", incr ? "inc" : "dec"); TODO: optimise */

	/* get here if op is *, +, - or ~ */
	gen_expr(e->lhs, tab);
	if(e->rhs){
		gen_expr(e->rhs, tab);
		asm_temp(1, "pop rcx");
		asm_temp(1, "pop rax");
		asm_temp(1, "%s rax, %s", instruct, rhs);
	}else{
		asm_temp(1, "pop rax");
		asm_temp(1, "%s rax", instruct);
	}

	asm_temp(1, "push rax");
}

void gen_expr_op_store(expr *e, symtable *stab)
{
	switch(e->op){
		case op_deref:
			/* a dereference */
			asm_temp(1, "push rax ; save val to store");

#ifdef FAST_ARRAY_DEREF
			/* check for a[n] aka *(a + n) */
			if(expr_kind(e->lhs, op)
			&& e->lhs->op == op_plus
			&& expr_kind(e->lhs->rhs, val))
			{
				gen_expr(e->lhs->lhs, stab);
				asm_temp(1, "pop rcx");
				asm_temp(1, "mov rbx, [rcx + %d]",
						e->lhs->rhs->val.iv.val);
				/* TODO */
				asm_indir(ASM_INDIR_...);
				return;
			}
#endif

			gen_expr(e->lhs, stab); /* skip over the *() bit */
			/* pointer on stack */

			/* move `pop` into `pop` */
			asm_temp(1, "pop rax ; ptr");
			asm_temp(1, "pop rbx ; val");

			asm_indir(ASM_INDIR_SET, e->tree_type, 'a', 'b', NULL);
			return;

		case op_struct_ptr:
			gen_expr(e->lhs, stab);

			asm_temp(1, "pop rbx ; struct addr");
			asm_temp(1, "add rbx, %d ; offset of member %s",
					e->rhs->tree_type->struct_offset,
					e->rhs->spel);
			asm_temp(1, "mov rax, [rsp] ; saved val");

			asm_indir(ASM_INDIR_SET, e->tree_type, 'b', 'a', NULL);

			return;

		case op_struct_dot:
			ICE("TODO: a.b");
			break;

		default:
			break;
	}
	ICE("invalid store-op %s", op_to_str(e->op));
}

void mutate_expr_op(expr *e)
{
	e->f_store = gen_expr_op_store;
	e->f_fold = fold_expr_op;
	e->f_const_fold = fold_const_expr_op;
}

expr *expr_new_op(enum op_type op)
{
	expr *e = expr_new_wrapper(op);
	e->op = op;
	return e;
}

void gen_expr_style_op(expr *e, symtable *stab)
{ (void)e; (void)stab; /* TODO */ }
