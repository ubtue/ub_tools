INC             = lib/include
LIB_SRCS        = lib/src
CCCFLAGS        = -g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -Wshadow -Ilib/include \
                  -ftrapv -Ilib/libstemmer/include \
                  -I/opt/shibboleth/include -pedantic -Wno-vla-extension -Wno-parentheses -march=native -O3 \
                  -I/usr/include/libxml2
CCCFLAGS_AMD64  = $(subst -march=native,,$(CCCFLAGS))
CCC             = clang++
MAKE_DEPS=iViaCore-mkdep
MAKE_DEPS_FLAGS = --output-program-rules

ifneq ("$(wildcard /etc/centos-release)","")
  LIBMARIADB=-L/usr/lib64/mysql/ -lmysqlclient
  LIBCURL=-L/opt/shibboleth/lib64/ -lcurl
else # Ubuntu version
  LIBMARIADB=-lmariadbclient
  LIBCURL=-lcurl
endif
LIBS = -Llib -lmarc -Llib/libstemmer -lstemmer -lxml2 -lpcre -lkyotocabinet -lmagic -lz \
       -larchive -lboost_graph $(LIBCURL) $(LIBMARIADB) -lrt -lssl -lcrypto -lpthread
LIBS_AMD64 = $(subst -lstemmer,-lstemmer_amd64,$(subst -lmarc,-lmarc_amd64,$(LIBS)))

PROGS := $(patsubst %.cc,%,$(wildcard *.cc))
CGI_PROGS=full_text_lookup
INSTALL_PROGS=$(filter-out %_test,$(PROGS)) *.sh

.PHONY: all install root_install clean

all: .deps $(PROGS)
amd64: .deps $(AMD64_PROGS)

%.o: %.cc
	@echo "Compiling $<..."
	@$(CCC) $(CCCFLAGS) $< -c

%_amd64.o: %.cc
	@echo "Compiling (AMD64) $<..."
	@$(CCC) $(CCCFLAGS_AMD64) $< -c -o $@

$(PROGS): % : %.o lib/libmarc.a
	@echo "Linking $@..."
	@$(CCC) $< -o $@ $(LIBS)

marc_filter_amd64: marc_filter_amd64.o lib/libmarc_amd64.a
	@echo "Linking $@..."
	@$(CCC) $< -o $@ $(LIBS_AMD64)

-include .deps
.deps: *.cc $(INC)/*.h Makefile
	$(MAKE_DEPS) $(MAKE_DEPS_FLAGS) -I $(INC) *.cc

lib/libmarc.a: $(wildcard lib/src/*.cc) $(wildcard lib/include/*.h)
	$(MAKE) -C lib

tests: $(patsubst %.cc,%,$(wildcard *test.cc))

install: $(INSTALL_PROGS)
	@echo "Installing programs..."
	cp $(INSTALL_PROGS) ~/bin/

root_install: $(INSTALL_PROGS)
	@echo "Installing programs and data..."
	cp $(INSTALL_PROGS) /usr/local/bin/
	mkdir --parents /var/lib/tuelib/bibleRef/
	cp data/books_of_the_bible_to_canonical_form.map \
            /var/lib/tuelib/bibleRef/books_of_the_bible_to_canonical_form.map
	cp data/books_of_the_bible_to_code.map \
            /var/lib/tuelib/bibleRef/books_of_the_bible_to_code.map

cgi_install: $(CGI_PROGS)
	mkdir --parents /var/www/cgi-bin
	cp full_text_lookup /var/www/cgi-bin/

local_clean:
	rm -f *.o *~ $(PROGS) .deps

clean: local_clean
	make -C lib clean
