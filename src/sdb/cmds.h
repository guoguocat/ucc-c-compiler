#ifndef CMDS_H
#define CMDS_H

noreturn void c_quit(tracee *child, char **argv);

int cmd_dispatch(tracee *child, char **cmd);

#endif
