#ifndef DAEMON_H
#define DAEMON_H

#define FIFO "cmd"

void daemon_init_dir(char *dir);
void daemon_fork(void);
void daemon_create_io(char *dir);

void sdb_vprintf(const char *, va_list);
void sdb_printf(const char *, ...);

#endif
