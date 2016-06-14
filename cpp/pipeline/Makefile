INC             = ../lib/include
LIB_SRCS        = ../lib/src
CCCFLAGS        = -g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -Wshadow -I../lib/include \
                  -ftrapv -I../lib/libstemmer/include \
                  -I/opt/shibboleth/include -pedantic -Wno-vla-extension -Wno-parentheses -march=native -O3 \
                  -I/usr/include/libxml2
CCC             = clang++
MAKE_DEPS=/usr/local/bin/iViaCore-mkdep
MAKE_DEPS_FLAGS = --output-program-rules

ifneq ("$(wildcard /etc/centos-release)","")
  LIBMARIADB=-L/usr/lib64/mysql/ -lmysqlclient
  LIBCURL=-L/opt/shibboleth/lib64/ -lcurl
else # Ubuntu version
  LIBMARIADB=-lmariadbclient
  LIBCURL=-lcurl
endif
LIBS = -L../lib -lmarc -L../lib/libstemmer -lstemmer -lxml2 -lpthread -lpcre -lcrypto -lkyotocabinet -lmagic -lz -lrt -lssl \
       -larchive $(LIBCURL) $(LIBMARIADB)

SOURCES = $(wildcard *.cc)
OBJS = $(filter-out Pipeline.o,$(SOURCES:.cc=.o))
PROGS := Pipeline
INSTALL_PROGS=$(filter-out %_test,$(PROGS)) *.sh

.PHONY: all install root_install local_clean clean

all: ../lib/libmarc.a $(OBJS) $(PROGS)

%.o: %.cc
	@echo "Compiling $<..."
	@$(CCC) $(CCCFLAGS) $< -c -o $@

$(PROGS): % : %.o ../lib/libmarc.a
	@echo "Linking $@..."
	@$(CCC) $< -o $@ $(OBJS) $(LIBS)

../lib/libmarc.a: $(wildcard ../lib/src/*.cc) $(wildcard ../lib/include/*.h)
	$(MAKE) -C ../lib

install: $(INSTALL_PROGS)
	cp $(INSTALL_PROGS) ~/bin/

root_install: $(INSTALL_PROGS)
	cp $(INSTALL_PROGS) /usr/local/bin/

local_clean:
	rm -f *.o *~ $(PROGS) .deps

clean: local_clean
	make -C ../lib clean