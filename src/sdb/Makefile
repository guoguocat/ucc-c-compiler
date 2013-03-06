include ../src_config.mk

DEP_FILE = Makefile.deps

CFLAGS += -std=c11 -Wmissing-prototypes # until merged into ../

OBJ = sdb.o util.o tracee.o prompt.o ptrace.o arch.o

sdb: ${OBJ}
	${CC} ${LDFLAGS} -o $@ ${OBJ}

clean:
	rm -f ${OBJ} sdb

deps:
	cc -MM *.c > ${DEP_FILE}

.PHONY: clean

include ${DEP_FILE}
