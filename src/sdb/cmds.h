#ifndef CMDS_H
#define CMDS_H

int c_quit(tracee *child, char **argv);

int cmd_dispatch(tracee *child, char **cmd);

#endif
