#ifndef DAEMON_H
#define DAEMON_H

#define FIFO "cmd"

void daemon_fork(void);
void daemon_init(char *dir);

void sdb_vprintf(const char *, va_list);
void sdb_printf(const char *, ...);

#endif
