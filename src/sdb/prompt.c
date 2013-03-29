#include <stdio.h>
#include <string.h>

#include "prompt.h"
#include "../util/dynarray.h"
#include "../util/alloc.h"

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
	int success = !!fgets(this, sizeof this, stdin);

	if(success){
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

	return NULL;
}
