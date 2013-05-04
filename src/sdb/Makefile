include ../src_config.mk
include config.mk

DEP_FILE = Makefile.deps

OBJ     = sdb.o util.o tracee.o daemon.o prompt.o arch/arch.o cmds.o \
          breakpoint.o signal.o \
          arch/${ARCH}/arch.o os/${OS_NAME}/ptrace.o os/${OS_NAME}/os.o \
          ../util/dynarray.o ../util/alloc.o

# use -Wmissing-prototypes here until merged into ../src_config.mk
CFLAGS  += ${WARN_FLAGS} \
           -fms-extensions -Wno-microsoft \
           -Wno-unused-parameter -Wmissing-prototypes

# we use a few range switches
CFLAGS2  = $(filter-out -pedantic,$(CFLAGS))
CPPFLAGS += -D_XOPEN_SOURCE -Iarch/${ARCH}

sdb: ${OBJ}
	${CC} ${LDFLAGS} -o $@ ${OBJ}
	${CODESIGN} -s sdb $@

.c.o:
	${CC} ${CPPFLAGS} ${CFLAGS2} -c -o $@ $<

simple: simple.s
	as -o simple.o simple.s
	ld -e _start -o simple simple.o

clean:
	rm -f ${OBJ} sdb

deps:
	cc ${CPPFLAGS} -MM *.c                                                 > ${DEP_FILE}
	echo                                                                  >> ${DEP_FILE}
	PRE=arch/         ; cc ${CPPFLAGS} -MM $$PRE/*.c | sed 's;^;'$$PRE';' >> ${DEP_FILE}
	echo                                                                  >> ${DEP_FILE}
	PRE=os/${OS_NAME}/; cc ${CPPFLAGS} -MM $$PRE/*.c | sed 's;^;'$$PRE';' >> ${DEP_FILE}

.PHONY: clean

include ${DEP_FILE}
