LIB := lib
include Makefile.inc


.PHONY: all .deps tests test install regular_install local_clean clean

all: .deps $(PROGS)
	$(MAKE) -C pipelines
	$(MAKE) -C marc_tools
	$(MAKE) -C cgi_progs
	$(MAKE) -C tests
	$(MAKE) -C test

%.o: %.cc Makefile
	@echo "Compiling $< $(OPTIMISATIONS_STATE)..."
	@$(CCC) $(CCCFLAGS) $< -c

$(PROGS): % : %.o lib/libubtue.a Makefile
	@echo "Linking $@..."
	@$(CCC) -rdynamic $(LD_FLAGS) $< -o $@ $(LIBS)

-include .deps
.deps: *.cc $(INC)/*.h
	$(MAKE_DEPS) -I $(INC) *.cc

lib/libubtue.a: $(wildcard lib/src/*.cc) $(wildcard lib/include/*.h)
	$(MAKE) -C lib

tests:
	$(MAKE) -C test

install: regular_install data_install
	make -C pipelines install
	make -C marc_tools install
	make -C cgi_progs install

regular_install: $(INSTALL_PROGS) Makefile
	@echo "Installing programs..."
	cp $(INSTALL_PROGS) /usr/local/bin/

data_install: Makefile data/*.conf data/*.map data/IxTheo_Notation.csv data/stopwords.* \
              data/subscriptions_email.ixtheo.template data/subscriptions_email.relbib.template \
              data/bibleRef/* data/crossref_downloader/* data/full_text_cache_monitor/* \
              data/translate_chainer/* data/zotero_cgi/*
	@echo "Installing data..."
	cp data/*.conf /usr/local/var/lib/tuelib/
	cp data/*.map /usr/local/var/lib/tuelib/
	cp data/IxTheo_Notation.csv /usr/local/var/lib/tuelib/
	cp data/stopwords.* /usr/local/var/lib/tuelib/
	cp data/subscriptions_email.ixtheo.template /usr/local/var/lib/tuelib/
	cp data/subscriptions_email.relbib.template /usr/local/var/lib/tuelib/
	cp -r data/bibleRef /usr/local/var/lib/tuelib/
	cp -r data/crossref_downloader /usr/local/var/lib/tuelib/
	cp -r data/full_text_cache_monitor /usr/local/var/lib/tuelib/
	cp -r data/translate_chainer /usr/local/var/lib/tuelib/
	cp -r data/zotero_cgi /usr/local/var/lib/tuelib/

local_clean:
	rm -f *.o *~ $(PROGS) .deps data/*~

clean: local_clean Makefile
	make -C pipelines clean
	make -C marc_tools clean
	make -C cgi_progs clean
	make -C lib clean
	make -C tests clean
	make -C test clean
