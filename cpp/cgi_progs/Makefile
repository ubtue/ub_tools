LIB := ../lib
INC = $(LIB)/include
include ../Makefile.inc
PROGS = display_translate_stats full_text_cache_monitor full_text_lookup ini_validator translate_chainer translator translator_ajax zotero_cgi restart_zts message_dispatcher
CGI_DIR = /var/www/cgi-bin
APACHE_START=systemctl start apache2
APACHE_STOP=systemctl stop apache2

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
	if test -f /etc/centos-release; then \
	    APACHE_START=systemctl start httpd; \
	    APACHE_STOP=systemctl stop httpd; \
	fi
	if [ $(shell pgrep -c -f message_dispatcher) -ne 0 ]; then \
	    $(APACHE_STOP); \
	    cp $(PROGS) $(CGI_DIR); \
	    $(APACHE_START); \
	else \
	    cp $(PROGS) $(CGI_DIR); \
	fi
	cp newsletter_uploader.py $(CGI_DIR)

clean:
	rm -f *.o *~ $(PROGS) .deps
