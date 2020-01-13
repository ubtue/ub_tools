LIB := ../lib
include ../Makefile.inc


.PHONY: all clean .deps

all: .deps $(PROGS)

%.o: %.cc
	@echo "Compiling $< $(OPTIMISATIONS_STATE)..."
	@$(CCC) $(CCCFLAGS) $< -c

$(PROGS): % : %.o ../lib/libubtue.a
	@echo "Linking $@..."
	@$(CCC) $(LD_FLAGS) $< -o $@ $(LIBS)

-include .deps
.deps: *.cc $(INC)/*.h Makefile
	$(MAKE_DEPS) -I $(INC) *.cc

../lib/libubtue.a: $(wildcard ../lib/src/*.cc) $(wildcard ../lib/include/*.h)
	$(MAKE) -C ../lib

clean:
	rm -f *.o *~ $(PROGS) .deps
