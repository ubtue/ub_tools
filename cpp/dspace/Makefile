LIB := ../lib
INC = $(LIB)/include
include ../Makefile.inc


.PHONY: all .deps install clean

all: .deps $(PROGS)

%.o: %.cc Makefile
	@echo "Compiling $< $(OPTIMISATIONS_STATE)..."
	@$(CCC) $(CCCFLAGS) $< -c

$(PROGS): % : %.o ../lib/libubtue.a Makefile
	@echo "Linking $@..."
	@$(CCC) -rdynamic $(LD_FLAGS) $< -o $@ $(LIBS)

-include .deps
.deps: *.cc $(INC)/*.h
	$(MAKE_DEPS) -I $(INC) *.cc

../lib/libubtue.a: $(wildcard ../lib/src/*.cc) $(wildcard ../lib/include/*.h)
	$(MAKE) -C ../lib

install: $(PROGS)
	@echo "Installing programs and scripts..."
	cp $(INSTALL_PROGS) /usr/local/bin/

clean:
	rm -f *.o *~ $(PROGS) .deps
