INC             = lib/include
CXXFLAGS        += -g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -Wshadow -Ilib/include -Ilib/libstemmer/include \
                   -pedantic -Wno-vla-extension -Wno-parentheses -march=native -O3 -I/usr/include/libxml2 -c
CXX             = clang++
MAKE_DEPS_FLAGS += --output-program-rules

ifneq ("$(wildcard /etc/centos-release)","")
  LIBMARIADB=-L/usr/lib64/mysql/ -lmysqlclient
  LIBCURL=-L/opt/shibboleth/lib64/ -lcurl
else # Ubuntu version
  LIBMARIADB=-lmariadbclient
  LIBCURL=-lcurl
endif
LIBS += -Llib -lmarc -lpcre -lcrypto -lkyotocabinet -lmagic -lrt -lssl $(LIBCURL) $(LIBMARIADB)

PROGS := $(patsubst %.cc,%,$(wildcard *.cc))

all: .deps $(PROGS)

-include .deps
.deps: $(PROGS) $(INC)/*.h Makefile
	iViaCore-mkdep $(MAKE_DEPS_FLAGS) -I $(INC) $(SRCS)

%.o: %.cc
	@echo "Compiling $<..."
	@$(CXX) $(CXXFLAGS) $< -c -o $@

%: %.o lib/libmarc.a
	@echo "Compiling and Linking $@..."
	@$(CXX) $< -o $@ $(LIBS)

.PHONY: all install root_install clean

install: all
	@echo "Installing programs..."
	cp $(PROGS) ~/bin/

root_install: all
	@echo "Installing programs..."
	cp $(PROGS) /usr/local/bin/

clean:
	rm -f *.o *~ $(PROGS)
