# File: Makefile -- makefile for programs in the .../libiViaCore/mkdep directory
#                   Copyright 2002-2006 Project Infomine.
#                   Copyright 2002-2006 The Regents of The University of California.

all: iViaCore-mkdep

.PHONY: all clean

install: iViaCore-mkdep
	@echo "Installing $< ..."
	@/usr/bin/install -c -s -m755 iViaCore-mkdep ~/bin/

root_install: iViaCore-mkdep
	@echo "Installing $< ..."
	@/usr/bin/install -c -s -m755 iViaCore-mkdep /usr/local/bin/

clean:
	rm -f *.o *~ $(PROGS)

iViaCore-mkdep: iViaCore-mkdep.cc
	@echo "Building $@ ..."
	@g++ -g -O3 -rdynamic -Wall -Wextra -Wshadow -Wparentheses -Wnon-virtual-dtor -Werror -Wno-ctor-dtor-privacy -Wstrict-null-sentinel -Wwrite-strings -Woverloaded-virtual -fmessage-length=0 -ftemplate-depth-200 -fno-rtti  $< -o $@
