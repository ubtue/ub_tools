TUEFIND_FLAVOUR ?= "unknown"
.PHONY: all install clean test install_configs

all:
	$(MAKE) -C cpp/lib/mkdep;
	$(MAKE) -C cpp;
	$(MAKE) -C cronjobs;
	if [ $(TUEFIND_FLAVOUR) != "unknown" ]; then\
	    $(MAKE) -C java;\
	fi

install: install_configs Makefile
	rm --recursive --force /usr/local/bin/*
	$(MAKE) -C cpp/lib/mkdep install;
	$(MAKE) -C cpp install;
	$(MAKE) -C cronjobs install;
	$(MAKE) -C bnb_yaz install;
	if [ $(TUEFIND_FLAVOUR) != "unknown" ]; then\
	    $(MAKE) -C java install;\
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
