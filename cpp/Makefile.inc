INC            = $(LIB)/include
LIB_SRCS       = $(LIB)/src
CCC            ?= clang++
CCCFLAGS       = -g -Wall -Wextra -Werror -Wunused-parameter -Wshadow -march=native -ftrapv \
                 -I$(LIB)/libstemmer/include -pedantic -I/opt/shibboleth/include -I/usr/include/libxml2 -I$(INC) \
                 -DETC_DIR='"/usr/local/var/lib/tuelib"'
ifneq ($(shell ls /etc/redhat-release),/etc/redhat-release)
  CCCFLAGS     += -DIS_CENTOS
endif

ifeq ($(origin UBTOOLS_CPP_NO_OPTIMISATIONS),undefined)
  LD_FLAGS =
  CCCFLAGS += -O3
  OPTIMISATIONS_STATE =
else
  LD_FLAGS = -fsanitize=address
  CCCFLAGS += -fsanitize=address
  CCCFLAGS += -D_GLIBCXX_DEBUG
  OPTIMISATIONS_STATE = (unoptimised)
endif

ifeq ($(CCC),clang++)
  CCCFLAGS += -std=gnu++11 -Wno-vla-extension -Wno-c++1y-extensions
else
  CCCFLAGS += -std=gnu++14
endif
OBJ            = .
MAKE_DEPS      = /usr/local/bin/iViaCore-mkdep
LIBS           = -L$(LIB) -lubtue -L$(LIB)/libstemmer -lstemmer -lxml2 -lpcre -lkyotocabinet -lmagic -lz \
                 -larchive -L/opt/shibboleth/lib64/ -lcurl -L/usr/lib64/mysql/ -lmysqlclient -lrt -lssl -lcrypto -lpthread -ldl \
                 -luuid -llept -ltesseract -lsqlite3 -lxerces-c
ifneq ("$(wildcard /usr/include/selinux)","")
  CCCFLAGS += -DHAS_SELINUX_HEADERS
  LIBS += -lselinux
endif
PROGS          = $(patsubst %.cc,%,$(wildcard *.cc))
SCRIPTS        = $(wildcard *.sh)
INSTALL_PROGS  = $(PROGS) $(SCRIPTS)
