INC        = ./include
SRC        = src
OBJ        = objs
SRCS       = $(SRC)/*.cc
TEMP       = $(addprefix $(OBJ)/,$(notdir $(wildcard $(SRC)/*.cc)))
OBJS       = $(TEMP:.cc=.o)
AMD64_OBJS = $(TEMP:.cc=_amd64.o)
CCC        ?= clang++
CCCFLAGS   = -g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -Wshadow -Wno-c++14-binary-literal \
             -march=native -O3 -Ilibstemmer/include -pedantic -I/opt/shibboleth/include -I/usr/include/libxml2 \
             -I$(INC) -ftrapv -DETC_DIR='"/var/lib/tuelib"'
ifeq ($(CCC),clang++)
  CCCFLAGS += -Wno-vla-extension
endif
AMD64_CCCFLAGS = $(subst -march=native,,$(CCCFLAGS))
MAKE_DEPS=iViaCore-mkdep

.PHONY: clean
.PRECIOUS: $(OBJ)/%.o

# Rules for building:
$(OBJ)/%.o: $(SRC)/%.cc
	@echo "Compiling $<..."
	@$(CCC) $(CCCFLAGS) $< -c -o $@

$(OBJ)/%_amd64.o: $(SRC)/%.cc
	@echo "Compiling (AMD64)$<..."
	@$(CCC) $(AMD64_CCCFLAGS) $< -c -o $@

all: /usr/local/bin/iViaCore-mkdep .deps libstemmer/libstemmer.a libubtue.a
amd64: .deps libstemmer/libstemmer_amd64.a libubtue_amd64.a

libubtue.a: $(OBJS)
	@echo "Linking $@..."
	@ar crs $@ $^

libubtue_amd64.a: $(AMD64_OBJS)
	@echo "Linking $@..."
	@ar crs $@ $^

libstemmer/libstemmer.a: $(wildcard libstemmer/src_c/*.c) $(wildcard libstemmer/include/*.h)
	$(MAKE) -C libstemmer

libstemmer/libstemmer_amd64.a: $(wildcard libstemmer/src_c/*.c) $(wildcard libstemmer/include/*.h)
	$(MAKE) -C libstemmer libstemmer_amd64.a

/usr/local/bin/iViaCore-mkdep:
	$(MAKE) -C mkdep install 

-include .deps
.deps: $(SRCS) $(INC)/*.h Makefile
	$(MAKE_DEPS) -I $(INC) $(SRCS)

clean:
	rm -f *~ $(PROGS) $(OBJS) *.a && cd libstemmer && make clean && rm -f libstemmer.a
