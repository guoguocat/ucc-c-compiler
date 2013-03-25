include ../src_config.mk

DEP_FILE = Makefile.deps

OS_NAME = $(shell uname -s | tr 'A-Z' 'a-z')
ARCH    = $(shell uname -m)
OBJ     = sdb.o util.o tracee.o prompt.o \
					os/ptrace.o os/${OS_NAME}/${OS_NAME}.o

CFLAGS   += -std=c11 -Wmissing-prototypes # until merged into ../
CPPFLAGS += -Iarch/${ARCH}

sdb: ${OBJ}
	${CC} ${LDFLAGS} -o $@ ${OBJ}

clean:
	rm -f ${OBJ} sdb

deps:
	cc ${CPPFLAGS} -MM *.c > ${DEP_FILE}
	PRE=os/           ; cc ${CPPFLAGS} -MM $$PRE/*.c | sed 's;^;'$$PRE';' >> ${DEP_FILE}
	PRE=os/${OS_NAME}/; cc ${CPPFLAGS} -MM $$PRE/*.c | sed 's;^;'$$PRE';' >> ${DEP_FILE}

.PHONY: clean

include ${DEP_FILE}
