LIB := ../lib
INC = $(LIB)/include
include ../Makefile.inc


.PHONY: install clean


install: download_krimdok_core.sh
	@echo "Installing script..."
	cp $^ /usr/local/bin/

clean:
	rm -f *~
