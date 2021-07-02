SUBDIRS = marc4j_check solr_plugins

all: 
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir || exit 1; \
	done

install:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir install || exit 1;\
	done	

clean: 
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean || exit 1; \
	done
