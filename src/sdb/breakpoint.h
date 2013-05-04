#ifndef BREAKPOINT_H
#define BREAKPOINT_H

typedef struct bkpt bkpt;

bkpt *bkpt_new(struct arch_proc *ap, addr_t a);
int   bkpt_place(bkpt *, struct arch_proc *ap, addr_t);

int  bkpt_enable(bkpt *);
int  bkpt_disable(bkpt *);

addr_t bkpt_addr(bkpt *);

#endif
