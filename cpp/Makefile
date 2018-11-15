INC            = lib/include
LIB_SRCS       = lib/src
CCC            ?= clang++
CCCFLAGS       = -g -Wall -Wextra -Werror -Wunused-parameter -Wshadow -march=native -ftrapv \
                 -Ilib/libstemmer/include -pedantic -I/opt/shibboleth/include -I/usr/include/libxml2 -I$(INC) \
                 -DETC_DIR='"/usr/local/var/lib/tuelib"'

ifeq ($(origin UBTOOLS_CPP_NO_OPTIMISATIONS),undefined)
  LD_FLAGS =
  CCCFLAGS += -O3
  OPTIMISATIONS_STATE =
else
  LD_FLAGS = -fsanitize=address
  CCCFLAGS += -fsanitize=address
  OPTIMISATIONS_STATE = (unoptimised)
endif

ifeq ($(CCC),clang++)
  CCCFLAGS += -std=gnu++11 -Wno-vla-extension -Wno-c++1y-extensions
else
  CCCFLAGS += -std=gnu++14
endif
OBJ            = .
MAKE_DEPS      = /usr/local/bin/iViaCore-mkdep
LIBS           = -Llib -lubtue -Llib/libstemmer -lstemmer -lxml2 -lpcre -lkyotocabinet -lmagic -lz \
                 -larchive -L/opt/shibboleth/lib64/ -lcurl -L/usr/lib64/mysql/ -lmysqlclient -lrt -lssl -lcrypto -lpthread -ldl \
                 -luuid -llept -ltesseract -lsqlite3 -lxerces-c
ifneq ("$(wildcard /usr/include/selinux)","")
  CCCFLAGS += -DHAS_SELINUX_HEADERS
  LIBS += -lselinux
endif
PROGS          = $(patsubst %.cc,%,$(wildcard *.cc))
CGI_PROGS      = full_text_cache_monitor full_text_lookup translate_chainer translator_ajax translator display_translate_stats zotero_cgi
INSTALL_PROGS  = $(PROGS) *.sh


.PHONY: all .deps tests test install regular_install cgi_install local_clean clean

all: .deps $(PROGS)
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

install: regular_install cgi_install data_install

regular_install: $(INSTALL_PROGS) Makefile
	@echo "Installing programs..."
	cp $(INSTALL_PROGS) /usr/local/bin/

cgi_install: $(CGI_PROGS) Makefile
	@echo "Installing CGI programs..."
	mkdir --parents /var/www/cgi-bin
	cp $(CGI_PROGS) /var/www/cgi-bin/

data_install: data/*.map data/*.html data/mytranslations_template.msg data/*.template data/*.conf \
              data/crossref_journal_list data/keyword_normalistion.map data/keyword_normalistion.map\
              data/keyword_normalistion.map Makefile
	@echo "Installing data..."
	mkdir --parents /usr/local/var/lib/tuelib/bibleRef/
	cp data/books_of_the_bible_to_canonical_form.map data/books_of_the_bible_to_code.map \
           data/pericopes_to_codes.map data/bible_aliases.map /usr/local/var/lib/tuelib/bibleRef/
	mkdir --parents /usr/local/var/lib/tuelib/translate_chainer
	cp data/done_translating.html data/translate.html data/translation_front_page.html \
           data/mytranslations_template.msg /usr/local/var/lib/tuelib/translate_chainer/
	cp -r data/full_text_cache_monitor /usr/local/var/lib/tuelib/
	cp -r data/zotero_cgi /usr/local/var/lib/tuelib/
	cp data/subscriptions_email.ixtheo.template /usr/local/var/lib/tuelib/
	cp data/subscriptions_email.relbib.template /usr/local/var/lib/tuelib/
	cp data/keyword_normalistion.map /usr/local/var/lib/tuelib/
	cp data/author_normalistion.map /usr/local/var/lib/tuelib/
	cp data/publisher_normalistion.map /usr/local/var/lib/tuelib/
	mkdir --parents /usr/local/var/lib/tuelib/crossref_downloader/
	cp data/crossref_journal_list /usr/local/var/lib/tuelib/crossref_downloader
	cp data/normalise_and_deduplicate_language.conf /usr/local/var/lib/tuelib/
	cp data/journal_alert_bundles.conf /usr/local/var/lib/tuelib/
	cp data/zeder_updater.conf /usr/local/var/lib/tuelib/

local_clean:
	rm -f *.o *~ $(PROGS) .deps data/*~

clean: local_clean Makefile
	make -C lib clean
	make -C tests clean
	make -C test clean
