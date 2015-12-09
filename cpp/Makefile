INC            = lib/include
CXXFLAGS       += -g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -Wshadow -Ilib/include -Ilib/libstemmer/include \
                  -pedantic -Wno-vla-extension -Wno-parentheses -march=native -O3 -I/usr/include/libxml2 -c
CXX            = clang++

LIBS           += -Llib -lmarc -lpcre -lcrypto -lkyotocabinet -lmagic -lrt -lssl -lcurl
MAKE_DEPS_FLAGS += --output-program-rules

PROGS := $(patsubst %.cc,%,$(wildcard *.cc))

%.o: %.cc
	@echo "Compiling $<..."
	@$(CXX) $(CXXFLAGS) $< -c -o $@

%: %.o lib/libmarc.a
	@echo "Compiling and Linking $@..."
	$(CXX) $< -o $@ $(LIBS)

all: .deps $(PROGS)

-include .deps
.deps: $(PROGS) $(INC)/*.h Makefile
	iViaCore-mkdep $(MAKE_DEPS_FLAGS) -I $(INC) $(SRCS)

.PHONY: all install root_install clean

install: all
	@echo "Installing programs..."
	cp $(PROGS) ~/bin/

root_install: all
	@echo "Installing programs..."
	cp $(PROGS) /usr/local/bin/

clean:
	rm -f *.o *~ $(PROGS) .deps
