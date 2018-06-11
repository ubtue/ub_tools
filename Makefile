TUEFIND_FLAVOUR ?= "unknown"
.PHONY: all install clean test install_configs

all:
	$(MAKE) -C cpp/lib/mkdep;
	$(MAKE) -C cpp;
	if [ $(TUEFIND_FLAVOUR) != "unknown" ]; then\
	    $(MAKE) -C solr_plugins;\
	    $(MAKE) -C solrmarc_mixin;\
	    $(MAKE) -C cronjobs;\
	fi

install: install_configs Makefile
	$(MAKE) -C cpp/lib/mkdep install;
	$(MAKE) -C cpp install;
	if [ $(TUEFIND_FLAVOUR) != "unknown" ]; then\
	    $(MAKE) -C solr_plugins install;\
	    $(MAKE) -C solrmarc_mixin install;\
	    $(MAKE) -C cronjobs install;\
	fi

clean:
	$(MAKE) -C cpp/lib/mkdep clean;
	$(MAKE) -C cpp clean;
	$(MAKE) -C cronjobs clean;
	$(MAKE) -C solr_plugins clean;
	$(MAKE) -C solrmarc_mixin clean;

test:
	$(MAKE) -C cpp/tests test;

install_configs:
	$(MAKE) -C /mnt/ZE020150/FID-Entwicklung/ install
