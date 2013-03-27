#ifndef BREAKPOINT_H
#define BREAKPOINT_H

typedef struct bkpt bkpt;

bkpt *bkpt_new(addr_t a, pid_t);
int   bkpt_place(bkpt *, pid_t, addr_t);

int  bkpt_enable(bkpt *);
int  bkpt_disable(bkpt *);

#endif
