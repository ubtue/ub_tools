INC            = lib/include
LIB_SRCS       = lib/src
CCC            ?= clang++
CCCFLAGS       = -g -Wall -Wextra -Werror -Wunused-parameter -Wshadow -march=native -O3 -ftrapv \
                 -Ilib/libstemmer/include -pedantic -I/opt/shibboleth/include -I/usr/include/libxml2 -I$(INC) \
                 -DETC_DIR='"/var/lib/tuelib"'
ifeq ($(CCC),clang++)
  CCCFLAGS += -std=gnu++11 -Wno-vla-extension -Wno-c++1y-extensions
else
  CCCFLAGS += -std=gnu++14
endif
CCCFLAGS_AMD64 = $(subst -march=native,,$(CCCFLAGS))
OBJ            = .
MAKE_DEPS      = /usr/local/bin/iViaCore-mkdep
LIBS           = -Llib -lubtue -Llib/libstemmer -lstemmer -lxml2 -lpcre -lkyotocabinet -lmagic -lz \
                 -larchive -lboost_graph -L/opt/shibboleth/lib64/ -lcurl -L/usr/lib64/mysql/ -lmysqlclient -lrt \
                 -lssl -lcrypto -lpthread -ldl
LIBS_AMD64     = $(subst -lstemmer,-lstemmer_amd64,$(subst -lubtue,-lubtue_amd64,$(LIBS)))

PROGS          = $(patsubst %.cc,%,$(wildcard *.cc))
AMD64_PROGS    = $(patsubst %.cc,%_amd64,$(wildcard *.cc))
CGI_PROGS      = full_text_lookup translate_chainer translator_ajax translator display_translate_stats
INSTALL_PROGS  = $(filter-out %_test,$(PROGS)) *.sh

.PHONY: all amd64 install clean

all: .deps $(PROGS)
	$(MAKE) -C tests

amd64: .deps $(AMD64_PROGS)

%.o: %.cc
	@echo "Compiling $<..."
	@$(CCC) $(CCCFLAGS) $< -c

%_amd64.o: %.cc
	@echo "Compiling (AMD64) $<..."
	@$(CCC) --static $(CCCFLAGS_AMD64) $< -c -o $@

$(PROGS): % : %.o lib/libubtue.a
	@echo "Linking $@..."
	@$(CCC) $< -o $@ $(LIBS)

$(AMD64_PROGS): % : %.o lib/libubtue.a
	@echo "Linking $@..."
	@$(CCC) -static-libgcc -static-libstdc++ $< -o $@ $(LIBS)

-include .deps
.deps: *.cc $(INC)/*.h Makefile
	$(MAKE_DEPS) -I $(INC) *.cc

lib/libubtue.a: $(wildcard lib/src/*.cc) $(wildcard lib/include/*.h)
	$(MAKE) -C lib

tests: $(patsubst %.cc,%,$(wildcard *test.cc))

install: regular_install cgi_install

regular_install: $(INSTALL_PROGS)
	@echo "Installing programs and data..."
	cp $(INSTALL_PROGS) /usr/local/bin/
	mkdir --parents /var/lib/tuelib/bibleRef/
	cp data/books_of_the_bible_to_canonical_form.map data/books_of_the_bible_to_code.map /var/lib/tuelib/bibleRef/
	mkdir --parents /var/lib/tuelib/translate_chainer
	cp data/done_translating.html  data/translate.html data/translation_front_page.html \
            data/mytranslations_template.msg /var/lib/tuelib/translate_chainer/
	cp data/subscriptions_email.ixtheo.template /var/lib/tuelib/
	cp data/subscriptions_email.relbib.template /var/lib/tuelib/
	mkdir --parents /var/lib/tuelib/crossref_downloader/
	cp data/crossref_journal_list /var/lib/tuelib/crossref_downloader/

cgi_install: $(CGI_PROGS)
	@echo "Installing CGI programs..."
	mkdir --parents /var/www/cgi-bin
	cp $(CGI_PROGS) /var/www/cgi-bin/

local_clean:
	rm -f *.o *~ $(PROGS) .deps

clean: local_clean
	make -C lib clean
