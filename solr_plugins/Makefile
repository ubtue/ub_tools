SUBDIRS = Schlagwortfolgensuche Bibelstellensuche PrefixSortFacetten MultiLanguageDocTransformer MultiLanguageQueryParser

all: 
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

install:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir install;\
	done	

clean: 
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done
