SUBDIRS = solr_plugins

all: 
	$(MAKE) -C cpp;
	$(MAKE) -C solr_plugins;
	$(MAKE) -C solrmarc_mixin;
	$(MAKE) -C cronjobs

install:
	$(MAKE) -C cpp install;
	$(MAKE) -C solr_plugins install;
	$(MAKE) -C solrmarc_mixin install;
	$(MAKE) -C cronjobs install

root_install:
	$(MAKE) -C cpp root_install;
	$(MAKE) -C cpp cgi_install;
	$(MAKE) -C solr_plugins install;
	$(MAKE) -C solrmarc_mixin install;
	$(MAKE) -C cronjobs root_install

clean: 
	$(MAKE) -C cpp clean;
	$(MAKE) -C cronjobs clean;
	$(MAKE) -C solr_plugins clean;
	$(MAKE) -C solrmarc_mixin clean;
# DO NOT DELETE
