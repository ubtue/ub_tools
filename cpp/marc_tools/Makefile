INC            = ../lib/include
LIB_SRCS       = ../lib/src
CCC            ?= clang++
CCCFLAGS       = -g -Wall -Wextra -Werror -Wunused-parameter -Wshadow -march=native -ftrapv \
                 -I../lib/libstemmer/include -pedantic -I/opt/shibboleth/include -I/usr/include/libxml2 -I$(INC) \
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
LIBS           = -L../lib -lubtue -L../lib/libstemmer -lstemmer -lxml2 -lpcre -lkyotocabinet -lmagic -lz \
                 -larchive -L/opt/shibboleth/lib64/ -lcurl -L/usr/lib64/mysql/ -lmysqlclient -lrt -lssl -lcrypto -lpthread -ldl \
                 -luuid -llept -ltesseract -lsqlite3 -lxerces-c
ifneq ("$(wildcard /usr/include/selinux)","")
  CCCFLAGS += -DHAS_SELINUX_HEADERS
  LIBS += -lselinux
endif
PROGS          = $(patsubst %.cc,%,$(wildcard *.cc))
INSTALL_PROGS  = $(PROGS) *.sh


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

install:
	@echo "Installing programs..."
	cp $(INSTALL_PROGS) /usr/local/bin/

clean:
	rm -f *.o *~ $(PROGS) .deps
