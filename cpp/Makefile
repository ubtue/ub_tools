INC             = lib/include
CCCFLAGS        = -g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -Wshadow -Ilib/include -Ilib/libstemmer/include \
                  -I/opt/shibboleth/include -pedantic -Wno-vla-extension -Wno-parentheses -march=native -O3 \
                  -I/usr/include/libxml2 -c
CCC             = clang++
MAKE_DEPS_FLAGS = --output-program-rules

ifneq ("$(wildcard /etc/centos-release)","")
  LIBMARIADB=-L/usr/lib64/mysql/ -lmysqlclient
  LIBCURL=-L/opt/shibboleth/lib64/ -lcurl
else # Ubuntu version
  LIBMARIADB=-lmariadbclient
  LIBCURL=-lcurl
endif
LIBS = -Llib -lmarc -Llib/libstemmer -lstemmer -lxml2 -lpthread -lpcre -lcrypto -lkyotocabinet -lmagic -lz -lrt -lssl $(LIBCURL) $(LIBMARIADB)

PROGS := $(patsubst %.cc,%,$(wildcard *.cc))
CGI_PROGS=full_text_lookup
INSTALL_PROGS=$(filter-out %_test,$(PROGS))

.PHONY: all install root_install clean

all: .deps $(PROGS)

%.o: %.cc
	@echo "Compiling $<..."
	@$(CCC) $(CCCFLAGS) $< -c -o $@

$(PROGS): % : %.o lib/libmarc.a
	@echo "Linking $@..."
	@$(CCC) $< -o $@ $(LIBS)

-include .deps
.deps: *.cc $(INC)/*.h Makefile
	iViaCore-mkdep $(MAKE_DEPS_FLAGS) -I $(INC) *.cc

lib/libmarc.a: $(wildcard lib/src/*.cc) $(wildcard lib/include/*.h)
	$(MAKE) -C lib

install: $(INSTALL_PROGS)
	@echo "Installing programs..."
	cp $(INSTALL_PROGS) ~/bin/

root_install: $(INSTALL_PROGS)
	@echo "Installing programs..."
	cp $(INSTALL_PROGS) /usr/local/bin/

cgi_install: $(CGI_PROGS)
	mkdir --parents /var/www/cgi-bin
	cp full_text_lookup /var/www/cgi-bin/

clean:
	rm -f *.o *~ $(PROGS) .deps
	make -C ./lib clean
