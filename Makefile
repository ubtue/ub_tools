.PHONY: all install clean

all: 
	$(MAKE) -C cpp/lib/mkdep;
	$(MAKE) -C cpp;
	$(MAKE) -C go;
	$(MAKE) -C solr_plugins;
	$(MAKE) -C solrmarc_mixin;
	$(MAKE) -C cronjobs

install:
	$(MAKE) -C cpp/lib/mkdep install;
	$(MAKE) -C cpp install;
	$(MAKE) -C go install;
	$(MAKE) -C solr_plugins install;
	$(MAKE) -C solrmarc_mixin install;
	$(MAKE) -C cronjobs install

clean: 
	$(MAKE) -C cpp/lib/mkdep clean;
	$(MAKE) -C cpp clean;
	$(MAKE) -C go clean;
	$(MAKE) -C cronjobs clean;
	$(MAKE) -C solr_plugins clean;
	$(MAKE) -C solrmarc_mixin clean;

test:
	$(MAKE) -C cpp/tests test;
# DO NOT DELETE
