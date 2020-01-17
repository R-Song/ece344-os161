# Makefile for parallelvm

SRCS=parallelvm.c
PROG=parallelvm
BINDIR=/testbin

include ../../defs.mk
include ../../mk/prog.mk

progdepend: say.h

say.h:
	ln -s ../console/say.h .

clean: cleanhere
cleanhere:
	rm -f say.h
