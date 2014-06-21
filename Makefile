CXX:=g++
CXXFLAGS:=-g -std=gnu++11 -Wall -W -Werror

CFLAGS:=-I. -fno-builtin -m32
CFLAGS+=-Wall -W -Werror -Wno-unused-function
CLANGFLAGS:=$(CFLAGS) -Wno-incompatible-library-redeclaration
TESTS:=cmp_lt cmp_le cmp_gt cmp_ge cmp_eq cmp_ne swapcase loop func print_int fizzbuzz malloc struct nullptr switch_op global puts

OPT:=1

ifeq ($(OPT), 0)
else
CFLAGS+=-O
#TESTS+=hello
endif

TBINS:=$(TESTS:%=test/%) lisp
TOBJS:=$(TBINS:=.o)
TASMS:=$(TBINS:=.s)
TBEFS:=$(TBINS:=.bef)

ALL=bc2bef befunge $(TOBJS) $(TASMS) $(TBEFS) $(TBINS)

all: $(ALL)

bc2bef.o: bc2bef.cc
	g++ -c $(CXXFLAGS) $< -o $@

bc2bef: bc2bef.o
	g++ $(CXXFLAGS) $< -lLLVM-3.3 -o $@

befunge: befunge.cc
	g++ $(CXXFLAGS) $< -o $@

$(TASMS): %.s: %.c Makefile libef.h
	clang -S $(CLANGFLAGS) -emit-llvm $< -o $@

$(TOBJS): %.o: %.c Makefile libef.h
	clang -c $(CLANGFLAGS) -emit-llvm $< -o $@

$(TBINS): %: %.c Makefile libef.h
	gcc -g $(CFLAGS) $< -o $@

$(TBEFS): %.bef: %.o bc2bef Makefile
	./bc2bef $< > $@ 2> err || (cat err; rm $@; exit 1)

test: all
	./test_bef.rb $(TESTS)

clean:
	rm -f $(ALL) err
