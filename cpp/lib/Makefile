INC    = ./include
SRC    = src
OBJ    = objs
SRCS   = $(SRC)/*.cc
TEMP   = $(addprefix $(OBJ)/,$(notdir $(wildcard $(SRC)/*.cc)))
OBJS   = $(TEMP:.cc=.o)
CCC    = g++
CCOPTS = -g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -Wshadow -march=native -Ilibstemmer/include -O3 \
         -pedantic -I/usr/include/libxml2 -I$(INC) -DETC_DIR='"/var/lib/tuelib"'
LDOPTS = -O3

.PHONY: clean
.PRECIOUS: $(OBJ)/%.o

# Rule for building:
$(OBJ)/%.o: $(SRC)/%.cc
	$(CCC) $(CCOPTS) $< -c -o $@


all: .deps libstemmer/libstemmer.a libmarc.a

libmarc.a: $(OBJS)
	@echo "Linking $@..."
	@ar crs $@ $^

libstemmer/libstemmer.a: $(wildcard libstemmer/src_c/*.c) $(wildcard libstemmer/include/*.h)
	$(MAKE) -C libstemmer

-include .deps
.deps: $(SRCS) $(INC)/*.h Makefile
	iViaCore-mkdep -I $(INC) $(SRCS)

clean:
	rm -f *~ $(PROGS) $(OBJS) *.a && cd libstemmer && make clean && rm -f libstemmer.a
