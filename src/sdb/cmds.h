#ifndef CMDS_H
#define CMDS_H

noreturn void c_quit(tracee *child);

int cmd_dispatch(tracee *child, const char *cmd);

#endif
