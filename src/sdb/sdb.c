#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "util.h"
#include "tracee.h"

static noreturn void
run_target(char **argv)
{
	tracee_traceme();

	execvp(*argv, argv);

	die("exec(\"%s\"):", *argv);
}

static void
dispatch(tracee *child, const char *cmd)
{
	switch(child->event){
		case TRACEE_TRAPPED:
			/* TODO */
			;
	}
}

static noreturn void
run_debugger(tracee *child)
{
	for(;;){
		tracee_wait(child);

		char cmd[64];
		if(!fgets(cmd, sizeof cmd, stdin)){
			putchar('\n');
			exit(0);
		}

		char *s;
		if((s = strchr(cmd, '\n')))
			*s = '\0';

		dispatch(child, cmd);
	}
}


int main(int argc, char **argv)
{
	tracee child;

	if(argc < 2){
		fprintf(stderr, "Usage: %s child-args...\n", *argv);
		return 1;
	}

	switch(tracee_create(&child)){
		case 0:
			run_target(argv + 1);
			break;

		default:
			run_debugger(&child);
	}
}
