LIB := ../lib
INC = $(LIB)/include
include ../Makefile.inc
PROGS = display_translate_stats full_text_cache_monitor full_text_lookup ini_validator translate_chainer translator translator_ajax zotero_cgi restart_zts message_dispatcher
CGI_DIR = /var/www/cgi-bin
APACHE_SYSTEMCTL_ARGUMENT := $(shell if [ -f /etc/debian_version ]; then echo "apache2"; elif [ -f /etc/centos-release ]; then echo "httpd"; else echo ""; fi)

.PHONY: all .deps install clean

all: .deps $(PROGS)

%.o: %.cc Makefile
	@echo "Compiling $< $(OPTIMISATIONS_STATE)..."
	@$(CCC) $(CCCFLAGS) $< -c

$(PROGS): % : %.o ../lib/libubtue.a Makefile
	@echo "Linking $@..."
	@$(CCC) -rdynamic $(LD_FLAGS) $< -o $@ $(LIBS)

-include .deps
.deps: *.cc $(INC)/*.h
	$(MAKE_DEPS) -I $(INC) *.cc

install: $(PROGS)
	@echo "Installing CGI programs..."
	mkdir --parents $(CGI_DIR)
ifndef APACHE_SYSTEMCTL_ARGUMENT
	$(error could not determine systemctl argument)
else
	if [ $(shell pgrep --count --full message_dispatcher) -ne 0 ]; then \
	    systemctl stop $(APACHE_SYSTEMCTL_ARGUMENT); \
	    cp $(PROGS) $(CGI_DIR); \
	    systemctl start $(APACHE_SYSTEMCTL_ARGUMENT); \
	else \
	    cp $(PROGS) $(CGI_DIR); \
	fi
	cp newsletter_uploader.py $(CGI_DIR)
endif

clean:
	rm -f *.o *~ $(PROGS) .deps
