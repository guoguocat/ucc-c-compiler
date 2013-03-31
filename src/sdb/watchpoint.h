#ifndef WATCHPOINT_H
#define WATCHPOINT_H

typedef struct watchpt watchpt;

int      watchpt_place(watchpt *, pid_t, addr_t, size_t);
watchpt *watchpt_new(pid_t, addr_t, size_t);

int watchpt_enable( watchpt *, int reg_idx);
int watchpt_disable(watchpt *, int reg_idx);

#endif
