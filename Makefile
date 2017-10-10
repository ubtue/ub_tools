.PHONY: all install clean test install_configs

all: 
	$(MAKE) -C cpp/lib/mkdep;
	$(MAKE) -C cpp;
	$(MAKE) -C solr_plugins;
	$(MAKE) -C solrmarc_mixin;
	$(MAKE) -C cronjobs

install: install_configs
	$(MAKE) -C cpp/lib/mkdep install;
	$(MAKE) -C cpp install;
	$(MAKE) -C solr_plugins install;
	$(MAKE) -C solrmarc_mixin install;
	$(MAKE) -C cronjobs install

clean: 
	$(MAKE) -C cpp/lib/mkdep clean;
	$(MAKE) -C cpp clean;
	$(MAKE) -C cronjobs clean;
	$(MAKE) -C solr_plugins clean;
	$(MAKE) -C solrmarc_mixin clean;

test:
	$(MAKE) -C cpp/tests test;

install_configs:
ifneq "$(wildcard /var/log/ixtheo/)" ""
	@echo "We have an IxTheo installation..."
	$(MAKE) -C /mnt/ZE020150/FID-Entwicklung/IxTheo/ install
else ifneq "$(wildcard /var/log/krimdok/)" ""
	@echo "We have a KrimDok installation..."
	$(MAKE) -C /mnt/ZE020150/FID-Entwicklung/KrimDok/ install
else
	$(error Did not find /var/log/ixtheo/ nor /var/log/krimdok/.)
endif
# DO NOT DELETE
