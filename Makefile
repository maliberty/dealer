# $Header: /home/henk/CVS/dealer/Makefile,v 1.15 1999/08/05 19:57:44 henk Exp $

CC      = gcc
CFLAGS = -Wall -pedantic -O2 -I. -DNDEBUG -DYY_NO_INPUT -Wno-deprecated-declarations -c
FLEX    = flex
YACC    = yacc

PROGRAM  = dealer
TARFILE  = ${PROGRAM}.tar
GZIPFILE = ${PROGRAM}.tar.gz

SRC  = dealer.c pbn.c c4.c getopt.c pointcount.c
LSRC = scan.l
YSRC = defs.y
HDR  = c4.h dealer.h getopt.h pbn.h pointcount.h tree.h

OBJ  = dealer.o defs.o scan.o pbn.o c4.o getopt.o pointcount.o
LOBJ = scan.c
YOBJ = defs.c


dealer: ${OBJ} ${LOBJ} ${YOBJ}
	${MAKE} -C Random lib
	$(CC) -o $@ ${OBJ} -L./Random -lgnurand

clean:
	rm -f ${OBJ} ${LOBJ} ${YOBJ} ${OBJ:.o=.d} ${PROGRAM} defs.h
	${MAKE} -C Examples clean
	${MAKE} -C Random   clean

tarclean: clean ${YOBJ}
	rm -f dealer
	rm -f ${TARFILE}  ${GZIPFILE}

tarfile: tarclean
	cd .. ; \
	tar cvf ${TARFILE} ${PROGRAM} --exclude CVS --exclude ${TARFILE}; \
	mv ${TARFILE} ${PROGRAM}
	gzip -f ${TARFILE}

test: dealer
	${MAKE} -C Examples test

format:
	clang-format -i $(SRC) $(HDR)

#
# Lex
#
.l.c:
	${FLEX} -t $< >$@

#
# Yacc
#
.y.c:
	${YACC} -d -o $@ $<

#
# C-code
#
.c.o:
	${CC} ${CFLAGS} -o $@ -MMD -MP $<

-include $(wildcard *.d)
