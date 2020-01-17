# Makefile for triplemat

SRCS=triplemat.c triple.c
PROG=triplemat
BINDIR=/testbin

include ../../defs.mk
include ../../mk/prog.mk

progdepend: triple.c triple.h

triple.c:
	ln -s ../tripleprog/triple.c .

triple.h:
	ln -s ../tripleprog/triple.h .

clean: cleanhere
cleanhere:
	rm -f triple.c triple.h

