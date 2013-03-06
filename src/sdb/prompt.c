#include <stdio.h>
#include <string.h>

#include "prompt.h"

/* TODO: term initialisation, char-by-char, completion, etc */
char *prompt()
{
	static char cmd[64];

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

		return cmd;
	}

	return NULL;
}
