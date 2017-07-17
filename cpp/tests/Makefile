INC            = ../lib/include
LIB_SRCS       = ../lib/src
CCCFLAGS       = -g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -Wshadow -I$(INC) \
                 -ftrapv -I../lib/libstemmer/include \
                 -I/opt/shibboleth/include -pedantic -Wno-vla-extension -Wno-parentheses -march=native \
                 -I/usr/include/libxml2  -DTEST=BOOST_AUTO_TEST_CASE # -fprofile-arcs -ftest-coverage
CCC            = clang++
OBJ            = .
MAKE_DEPS      = /usr/local/bin/iViaCore-mkdep
LIBS           = -L../lib -lubtue -L../lib/libstemmer -lstemmer -lxml2 -lpcre -lkyotocabinet -lmagic -lz \
                 -larchive -lboost_graph -L/opt/shibboleth/lib64/ -lcurl -L/usr/lib64/mysql/ -lmysqlclient -lrt \
                 -lssl -lcrypto -lpthread -lboost_unit_test_framework -ldl # -lgcov --coverage

PROGS          = $(patsubst %.cc,%,$(wildcard *.cc))
CGI_PROGS      = full_text_lookup translate_chainer
INSTALL_PROGS  = $(filter-out %_test,$(PROGS)) *.sh

.PHONY: all amd64 install clean

all: .deps $(PROGS)

%.o: %.cc
	@echo "Compiling $<..."
	@$(CCC) $(CCCFLAGS) $< -c

$(PROGS): % : %.o ../lib/libubtue.a
	@echo "Linking $@..."
	@$(CCC) $< -o $@ $(LIBS)

-include .deps
.deps: *.cc $(INC)/*.h Makefile
	$(MAKE_DEPS) -I $(INC) *.cc

../lib/libubtue.a: $(wildcard ../lib/src/*.cc) $(wildcard ../lib/include/*.h)
	$(MAKE) -C ../lib

install: regular_install cgi_install

test: all
	@$(foreach prog,$(PROGS), echo "Running" $(prog) ; ./$(prog); echo "------------------"; )

clean:
	rm -f *.o *~ $(PROGS) .deps *.gcda *.gcno *.info
	rm -rf output

