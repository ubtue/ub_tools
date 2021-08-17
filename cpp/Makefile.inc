MAKEFLAGS      += --warn-undefined-variables
CCC            ?= clang++
CCCFLAGS       = -g -Wall -Wextra -Werror -Wunused-parameter -Wshadow -Wno-parentheses -march=native -ftrapv \
                 -pedantic -I/opt/shibboleth/include -I/usr/include/libxml2 -I$(INC) \
                 -DETC_DIR='"/usr/local/var/lib/tuelib"' -DUB_DEFAULT_LOCALE='"en_US.UTF-8"' \
                 -I$(shell pg_config --includedir)
LIBS           = -L$(LIB) -lubtue -lstemmer -lxml2 -lpcre -lmagic -lz \
                 -larchive -L/opt/shibboleth/lib64/ -lcurl -L/usr/lib64/mysql/ -lmysqlclient -lrt -lssl -lcrypto -lpthread -ldl \
                 -luuid -llept -ltesseract -lsqlite3 -lxerces-c -ldb -lpq

ifeq ($(origin UBTOOLS_CPP_NO_OPTIMISATIONS),undefined)
  LD_FLAGS = -flto
  CCCFLAGS += -O3 -funroll-loops -fomit-frame-pointer
  OPTIMISATIONS_STATE =
else
  LD_FLAGS = -fsanitize=thread
  CCCFLAGS += -fsanitize=thread
  CCCFLAGS += -D_GLIBCXX_DEBUG
  OPTIMISATIONS_STATE = (unoptimised)
endif

ifeq ($(CCC),clang++)
  CCCFLAGS += -std=gnu++2a -Wno-vla-extension #-Wno-c++1y-extensions
else
  CCCFLAGS += -std=gnu++2a
endif
OBJ            = .
MAKE_DEPS      = /usr/local/bin/iViaCore-mkdep
ifneq ("$(wildcard /usr/include/selinux)","")
  CCCFLAGS += -DHAS_SELINUX_HEADERS
  LIBS += -lselinux
endif
PROGS          = $(patsubst %.cc,%,$(wildcard *.cc))
SCRIPTS        = $(wildcard *.sh)
INSTALL_PROGS  = $(PROGS) $(SCRIPTS)
INCROND_DIR    = /etc/incron.d
