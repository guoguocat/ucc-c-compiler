#define _POSIX_SOURCE
#include <stdio.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

#include "io.h"

FILE *fopen_w_chmod(const char *path, enum open_mode mode)
{
	int fd;
	FILE *f;
	int flags = O_EXCL | O_CREAT;

	switch(mode){
		case OPEN_RW:
			flags |= O_RDWR;
			break;
		case OPEN_APPEND:
			flags |= O_APPEND;
			/* fall */
		case OPEN_W:
			flags |= O_WRONLY;
			break;
	}

	fd = open(path, flags, 0600);
	if(fd == -1)
		return NULL;

	f = fdopen(fd, mode == OPEN_RW_SEEK ? "w+" : "a");
	if(!f)
		close(fd);
	return f;
}
