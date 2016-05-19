INC    = ./include
SRC    = src
OBJ    = objs
SRCS   = $(SRC)/*.cc
TEMP   = $(addprefix $(OBJ)/,$(notdir $(wildcard $(SRC)/*.cc)))
OBJS   = $(TEMP:.cc=.o)
CCC    = clang++
CCOPTS = -g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -Wshadow -march=native -Ilibstemmer/include -O3 \
         -pedantic -Wno-vla-extension -I/opt/shibboleth/include -I/usr/include/libxml2 -I$(INC) \
         -fsanitize=undefined-trap -fsanitize-undefined-trap-on-error -ftrapv -DETC_DIR='"/var/lib/tuelib"'
LDOPTS = -O3
MAKE_DEPS=/usr/local/bin/iViaCore-mkdep

ifneq ("$(wildcard /etc/centos-release)","")
  LZ4_PATH=/bin/lz4
else # Ubuntu version
  LZ4_PATH=/usr/bin/lz4
endif

.PHONY: clean
.PRECIOUS: $(OBJ)/%.o

# Rule for building:
$(OBJ)/%.o: $(SRC)/%.cc
	$(CCC) $(CCOPTS) $< -c -o $@ -DLZ4_PATH=\"$(LZ4_PATH)\"

all: .deps libstemmer/libstemmer.a libmarc.a

libmarc.a: $(OBJS)
	@echo "Linking $@..."
	@ar crs $@ $^

libstemmer/libstemmer.a: $(wildcard libstemmer/src_c/*.c) $(wildcard libstemmer/include/*.h)
	$(MAKE) -C libstemmer

-include .deps
.deps: $(SRCS) $(INC)/*.h Makefile
	$(MAKE_DEPS) -I $(INC) $(SRCS)

clean:
	rm -f *~ $(PROGS) $(OBJS) *.a && cd libstemmer && make clean && rm -f libstemmer.a
