# $Header: /home/henk/CVS/dealer/Makefile,v 1.15 1999/08/05 19:57:44 henk Exp $

CXX     = g++
CFLAGS = -Wall -pedantic -O2 -I. -DNDEBUG -DYY_NO_INPUT -Wno-deprecated-declarations --std=c++11 -c
FLEX    = flex
YACC    = bison

PROGRAM  = dealer
TARFILE  = ${PROGRAM}.tar
GZIPFILE = ${PROGRAM}.tar.gz

SRC  = dealer.cpp pbn.cpp c4.cpp getopt.cpp pointcount.cpp
LSRC = scan.l
YSRC = defs.y
HDR  = c4.h dealer.h getopt.h pbn.h pointcount.h tree.h

OBJ  = dealer.o defs.o scan.o pbn.o c4.o getopt.o pointcount.o
LOBJ = scan.cpp
YOBJ = defs.cpp


dealer: ${OBJ} ${LOBJ} ${YOBJ}
	${MAKE} -C Random lib
	$(CC) -o $@ ${OBJ} -L./Random -lgnurand

clean:
	rm -f ${OBJ} ${LOBJ} ${YOBJ} ${OBJ:.o=.d} ${PROGRAM} defs.hpp
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
%.cpp: %.l
	${FLEX} -o $@ $<

#
# Yacc
#
%.cpp: %.y
	${YACC} -d -o $@ $<

#
# C++-code
#
%.o: %.cpp
	${CXX} ${CFLAGS} -o $@ -MMD -MP $<

# The C skeleton has register usage that generates warnings in c++11
scan.o : CFLAGS += -Wno-deprecated-register

-include $(wildcard *.d)
