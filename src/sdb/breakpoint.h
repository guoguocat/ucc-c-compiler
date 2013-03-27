#ifndef BREAKPOINT_H
#define BREAKPOINT_H

typedef struct bkpt bkpt;

bkpt *bkpt_new(addr_t a, pid_t);
void bkpt_place(bkpt *, addr_t, pid_t);
int  bkpt_enable(bkpt *);
int  bkpt_disable(bkpt *);

#endif
