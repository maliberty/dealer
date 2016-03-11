SRC      = $(wildcard *.cpp)
OBJ      = $(subst .cpp,.o,$(SRC))
CXX      = g++
CXXFLAGS = -c -Wall -O2 -I.
HDR      = ansidecl.h
LIB      = libgnurand.a
AR       = ar
ARFLAGS  = rcs

help:
	@echo "Makefile tags:"
	@echo
	@echo "lib:   Make ${LIB}"
	@echo "clean: Remove ${OBJ}"
	@echo

lib: ${LIB}


clean:
	-rm ${OBJ} ${LIB}

%.o : %.cpp ${HDR}
	${CXX} ${CXXFLAGS} -o $@ $<

${LIB}: ${OBJ} ${HDR}
	${AR} ${ARFLAGS} $@ ${OBJ}

__random.o: ${HDR}
rand.o:     ${HDR}
srand.o:    ${HDR}
