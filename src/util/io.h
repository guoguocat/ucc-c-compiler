#ifndef IO_H
#define IO_H

enum open_mode
{
	OPEN_W,
	OPEN_RW,
	OPEN_APPEND
};

FILE *fopen_w_chmod(const char *path, enum open_mode);

#endif
