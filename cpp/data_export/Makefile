LIB := ../lib
INC = $(LIB)/include
include ../Makefile.inc


.PHONY: all install clean

all: $(PROGS)

install: $(PROGS)
	@echo "Installing programs..."
	cp $(INSTALL_PROGS) /usr/local/bin/

clean:
	rm -f *.o *~ $(PROGS)
