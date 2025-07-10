LIB := ../lib
INC = $(LIB)/include
include ../Makefile.inc
PROGS = display_translate_stats full_text_cache_monitor full_text_lookup ini_validator translate_chainer translator translator_ajax translator_statistics zotero_cgi restart_zts message_dispatcher
CGI_DIR = /var/www/cgi-bin
APACHE_SYSTEMCTL_ARGUMENT := $(shell if [ -f /etc/debian_version ]; then echo "apache2"; elif [ -f /etc/centos-release ]; then echo "httpd"; else echo ""; fi)

.PHONY: all .deps install clean

all: .deps $(PROGS) record_aggregator_server

%.o: %.cc Makefile
	@echo "Compiling $< $(OPTIMISATIONS_STATE)..."
	@$(CCC) $(CCCFLAGS) $< -c

$(PROGS): % : %.o ../lib/libubtue.a Makefile
	@echo "Linking $@..."
	@$(CCC) -rdynamic $(LD_FLAGS) $< -o $@ $(LIBS)

record_aggregator_server: record_aggregator_server.o ../lib/libubtue.a
	@echo "Linking $@ with Boost..."
	@$(CCC) -rdynamic $(LD_FLAGS) record_aggregator_server.o -o $@ -lboost_system -lboost_filesystem -lpthread $(LIBS)

-include .deps
.deps: *.cc $(INC)/*.h
	$(MAKE_DEPS) -I $(INC) *.cc

../lib/libubtue.a: $(wildcard ../lib/src/*.cc) $(wildcard ../lib/include/*.h)
	$(MAKE) -C ../lib

install: $(PROGS) record_aggregator_server
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
