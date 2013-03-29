#ifndef BREAKPOINT_H
#define BREAKPOINT_H

typedef struct bkpt bkpt;

bkpt *bkpt_new(pid_t pid, addr_t a);
int   bkpt_place(bkpt *, pid_t, addr_t);

int  bkpt_enable(bkpt *);
int  bkpt_disable(bkpt *);

#endif
