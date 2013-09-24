#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "../../../util/where.h"
#include "../../../util/alloc.h"
#include "../../../util/assert.h"
#include "../../../util/dynarray.h"

#include "../../data_structs.h"
#include "../../type_ref.h"

#include "../vstack.h"
#include "bb.h"
#include "io.h"
#include "defs.h"

#include "../out.h" /* needed for fork/phi ops */
#include "../lbl.h"
#include "../impl_flow.h" /* impl_{jmp,lbl} */


basic_blk *bb_new(char *label)
{
	basic_blk *bb = umalloc(sizeof *bb);
	bb->type = bb_norm;
	bb->lbl = out_label_code(label);
	return bb;
}

static basic_blk_phi *bb_new_phi(void)
{
	basic_blk_phi *phi = umalloc(sizeof *phi);
	phi->type = bb_phi;
	return phi;
}

basic_blk *bb_phi_next(basic_blk_phi *phi)
{
	if(!phi->next)
		phi->next = bb_new("phi");
	return phi->next;
}

void bb_addv(basic_blk *bb, const char *fmt, va_list l)
{
	char *insn = ustrvprintf(fmt, l);

	UCC_ASSERT(bb->type == bb_norm, "insn on non-normal block");

	dynarray_add(&bb->insns, insn);
}

void bb_commentv(basic_blk *bb, const char *fmt, va_list l)
{
	char **plast;
	size_t len;

	bb_addv(bb, fmt, l);

	plast = &bb->insns[dynarray_count(bb->insns)-1];

	len = strlen(*plast);
	*plast = urealloc1(*plast, len + 3); /* \0, "# " */
	memmove(*plast + 2, *plast, len + 2);

	memcpy(*plast, "# ", 2);
}

void bb_add(basic_blk *bb, const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
	bb_addv(bb, fmt, l);
	va_end(l);
}

void bb_pop_to(basic_blk *from, basic_blk *to)
{
	(void)from;
	ICW("TODO");
	bb_add(to, "TODO: pop_to");
}

void bb_terminates(basic_blk *bb)
{
	(void)bb;
}

void bb_split(
		basic_blk *exp,
		basic_blk *b_true,
		basic_blk *b_false,
		basic_blk_phi **pphi)
{
	struct basic_blk_fork *fork = umalloc(sizeof *fork);

	UCC_ASSERT(!exp->next, "can't split - already have a .next");
	exp->next = (basic_blk *)fork;

	/* need flag or const for our jump */
	impl_flag_or_const(vtop, exp);
	if((fork->on_const = (vtop->type == V_CONST_I))){
		fork->bits.const_t = !!vtop->bits.val_i;
	}else{
		/* otherwise it's a flag */
		UCC_ASSERT(vtop->type == V_FLAG, "impl_flag_or_const?");
		fork->bits.flag = vtop->bits.flag;
	}
	out_pop(exp);

	fork->type = bb_fork;

	fork->exp = exp;
	fork->btrue = b_true, fork->bfalse = b_false;
	fork->phi = *pphi = bb_new_phi();
}

void bb_link_forward(basic_blk *from, basic_blk *to)
{
	UCC_ASSERT(!from->next, "forward link present");
	from->next = to;
}

void bb_phi_incoming(basic_blk_phi *to, basic_blk *from)
{
	/* from doesn't link to the phi node */
	dynarray_add(&to->incoming, from);
}