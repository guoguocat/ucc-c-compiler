#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

#include "tracee.h"
#include "cmds.h"
#include "prompt.h"

static noreturn void
run_target(char **argv)
{
	tracee_traceme();

	execvp(*argv, argv);

	die("exec(\"%s\"):", *argv);
}

static noreturn void
run_debugger(tracee *child)
{
	for(;;){
		tracee_wait(child);

		switch(child->event){
			case TRACEE_KILLED:
				printf("exited with code %d\n", child->exit_code);
				break;

			case TRACEE_SIGNALED:
				printf("signaled with signal %d\n", child->sig);
				break;

			case TRACEE_TRAPPED:
				printf("trapped\n");
				break;
		}

prompt:
		fputs("(sdb) ", stdout);

		char *cmd;
		if(!(cmd = prompt())){
			putchar('\n');
			c_quit(child);
		}

		if(!cmd_dispatch(child, cmd))
			goto prompt;
	}
}


int
main(int argc, char **argv)
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
