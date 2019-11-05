INC        = ./include
SRC        = src
OBJ        = objs
STATIC_OBJ = static_objs
SRCS       = $(SRC)/*.cc
TEMP       = $(addprefix $(OBJ)/,$(notdir $(wildcard $(SRC)/*.cc)))
STATIC_TEMP= $(addprefix $(STATIC_OBJ)/,$(notdir $(wildcard $(SRC)/*.cc)))
OBJS       = $(TEMP:.cc=.o)
STATIC_OBJS= $(STATIC_TEMP:.cc=_static.o)
CCC        ?= clang++
CCCFLAGS   = -g -Wall -Wextra -Werror -Wunused-parameter -Wshadow \
             -march=$(shell gcc -march=broadwell -Q --help=target | grep -- '-mtune=' | cut -f3) -ftrapv \
             -Ilibstemmer/include -pedantic -I/opt/shibboleth/include -I/usr/include/libxml2 -I$(INC) \
             -DETC_DIR='"/usr/local/var/lib/tuelib"'

ifeq ($(origin UBTOOLS_CPP_NO_OPTIMISATIONS),undefined)
  CCCFLAGS += -Ofast -funroll-loops -fomit-frame-pointer
  OPTIMISATIONS_STATE =
else
  CCCFLAGS += -fsanitize=address
  CCCFLAGS += -D_GLIBCXX_DEBUG
  OPTIMISATIONS_STATE = (unoptimised)
endif

ifeq ($(CCC),clang++)
  ifeq ("$(wildcard /etc/centos-release)","") # Not on CentOS!
    CCCFLAGS += -std=gnu++11 -Wno-vla-extension -Wno-c++1y-extensions -Wno-c++1z-extensions
  else
    CCCFLAGS += -std=gnu++11 -Wno-vla-extension -Wno-c++1y-extensions
  endif
else
  CCCFLAGS += -std=gnu++14
endif
ifneq ("$(wildcard /usr/include/selinux)","")
  CCCFLAGS += -DHAS_SELINUX_HEADERS
endif
MAKE_DEPS=iViaCore-mkdep

.PHONY: clean
.PRECIOUS: $(OBJ)/%.o $(STATIC_OBJ)/%_static.o

# Rules for building:
$(OBJ)/%.o: $(SRC)/%.cc Makefile
	@echo "Compiling $< $(OPTIMISATIONS_STATE)..."
	@$(CCC) $(CCCFLAGS) $< -c -o $@
$(STATIC_OBJ)/%_static.o: $(SRC)/%.cc Makefile
	@echo "Compiling static $< $(OPTIMISATIONS_STATE)..."
	@$(CCC) -static $(filter-out -march=native, $(CCCFLAGS)) $< -c -o $@

all: /usr/local/bin/iViaCore-mkdep .deps libstemmer/libstemmer.a libubtue.a

libubtue.a: $(OBJS) Makefile
	@echo "Generating $@..."
	@ar crs $@ $(OBJS)

libubtue_static.a: $(STATIC_OBJS) Makefile
	@echo "Generating $@..."
	@ar crs $@ $(STATIC_OBJS)

libstemmer/libstemmer.a: $(wildcard libstemmer/src_c/*.c) $(wildcard libstemmer/include/*.h)
	$(MAKE) -C libstemmer

libstemmer/libstemmer_amd64.a: $(wildcard libstemmer/src_c/*.c) $(wildcard libstemmer/include/*.h)
	$(MAKE) -C libstemmer libstemmer_amd64.a

/usr/local/bin/iViaCore-mkdep:
	$(MAKE) -C mkdep install

-include .deps
.deps: $(SRCS) $(INC)/*.h Makefile
	$(MAKE_DEPS) -I $(INC) $(SRCS)

-include .static_deps
.static_deps: $(SRCS) $(INC)/*.h Makefile
	$(MAKE_DEPS) --static-objects -I $(INC) $(SRCS)

clean:
	rm -f *~ $(OBJ)/*.o $(STATIC_OBJ)/*.o *.a .deps .static_deps && cd libstemmer && make clean && rm -f libstemmer.a
