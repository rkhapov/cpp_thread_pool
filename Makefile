EXECUTABLE=pool

CXX=clang++
LD=clang++

LDFLAGS=-lm -lpthread
WARN_OPTS=-Wall -Werror -pedantic -Wno-deprecated-declarations

INCLUDES=

COMPILER_FLAGS=

CXXFLAGS=$(WARN_OPTS) $(INCLUDES) $(COMPILER_FLAGS) \
		-std=c++17 -O2

LINK_EXECUTABLE=$(LD) $(LDFLAGS) -o $@ $^

COMPILE_CXX_SRC=$(CXX) $(CXXFLAGS) -c -o $@ $<

all: $(EXECUTABLE)

clean:
	rm $(EXECUTABLE)
	rm *.o

$(EXECUTABLE): main.o
	$(LINK_EXECUTABLE)

main.o: main.cpp pool.h
	$(COMPILE_CXX_SRC)

