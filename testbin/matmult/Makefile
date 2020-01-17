# Makefile for matmult

SRCS=matmult.c
PROG=matmult
BINDIR=/testbin

include ../../defs.mk
include ../../mk/prog.mk

progdepend: say.h

say.h:
	ln -s ../console/say.h .

clean: cleanhere
cleanhere:
	rm -f say.h

