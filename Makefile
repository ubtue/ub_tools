TUEFIND_FLAVOUR ?= "unknown"
.PHONY: all install clean test install_configs

all:
	$(MAKE) -C cpp/lib/mkdep;
	$(MAKE) -C cpp;
	if [ $(TUEFIND_FLAVOUR) != "unknown" ]; then\
	    $(MAKE) -C java;\
	    $(MAKE) -C cronjobs;\
	fi

install: install_configs Makefile
	rm --force /usr/local/bin/*
	$(MAKE) -C cpp/lib/mkdep install;
	$(MAKE) -C cpp install;
	if [ $(TUEFIND_FLAVOUR) != "unknown" ]; then\
	    $(MAKE) -C java install;\
	    $(MAKE) -C cronjobs install;\
	fi

clean:
	$(MAKE) -C cpp/lib/mkdep clean;
	$(MAKE) -C cpp clean;
	$(MAKE) -C cronjobs clean;
	$(MAKE) -C java clean;

test:
	$(MAKE) -C cpp/tests test;

install_configs:
	$(MAKE) -C /mnt/ZE020150/FID-Entwicklung/ install
