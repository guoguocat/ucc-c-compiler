include ../src_config.mk

CFLAGS += -std=c11 -Wmissing-prototypes # until merged into ../

OBJ = sdb.o util.o tracee.o prompt.o ptrace.o

sdb: ${OBJ}
	${CC} ${LDFLAGS} -o $@ ${OBJ}

clean:
	rm -f ${OBJ} sdb

.PHONY: clean
