LIB := ../lib
INC = $(LIB)/include
include ../Makefile.inc
PROGS = full_text_cache_monitor full_text_lookup translate_chainer translator_ajax translator display_translate_stats zotero_cgi


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

install: $(PROGS)
	@echo "Installing CGI programs..."
	mkdir --parents /var/www/cgi-bin
	cp $(PROGS) /var/www/cgi-bin/

clean:
	rm -f *.o *~ $(PROGS) .deps
