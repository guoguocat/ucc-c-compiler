#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "util.h"

#include "prompt.h"
#include "daemon.h"

#include "../util/dynarray.h"
#include "../util/alloc.h"

static int prompt_fd = -1;

/* TODO: term initialisation, char-by-char, completion, etc */
static char **parse(char *cmd)
{
	char **r = NULL;
	char *p;

	for(p = strtok(cmd, " \t"); p; p = strtok(NULL, " \t"))
		dynarray_add((void ***)&r, ustrdup(p));

	return r;
}

char **prompt()
{
	static char cmd[64] = "help";
	char this[sizeof cmd];

retry:
	fflush(stdout);

	if(prompt_fd == -1){
		prompt_fd = open(FIFO, O_RDONLY);
		if(prompt_fd == -1)
			die("open(\"%s\"):", FIFO);
	}

	errno = 0;
	ssize_t n = read(prompt_fd, this, sizeof this);

	switch(n){
		case -1:
			die("read():");
		case 0:
			close(prompt_fd), prompt_fd = -1;
			goto retry;
		default:
		{
			char *s;
			if((s = strchr(this, '\n')))
				*s = '\0';

			if(*this){
				strcpy(cmd, this);
			}else{
				/* nothing, use previous */
				printf("[%s]\n", cmd);
			}

			return parse(cmd);
		}
	}

	return NULL;
}
