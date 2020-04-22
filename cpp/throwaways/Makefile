LIB          := ../lib
INC          = $(LIB)/include
include ../Makefile.inc


.PHONY: all .deps .static_deps debs install clean

all: .deps $(PROGS)

%.o: %.cc Makefile
	@echo "Compiling $< $(OPTIMISATIONS_STATE)..."
	@$(CCC) $(CCCFLAGS) $< -c

$(PROGS): % : %.o ../lib/libubtue.a Makefile
	@echo "Linking $@..."
	@$(CCC) -rdynamic $(LD_FLAGS) $< -o $@ $(LIBS)

-include .deps
.deps: *.cc $(INC)/*.h Makefile
	$(MAKE_DEPS) -I $(INC) *.cc

../lib/libubtue.a: $(wildcard ../lib/src/*.cc) $(wildcard ../lib/include/*.h)
	$(MAKE) -C ../lib

clean:
	rm -f *.o *~ $(PROGS) $(STATIC_PROGS) .deps .static_deps debs/*.deb
