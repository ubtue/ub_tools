SUBDIRS = RangedSearches PrefixSortFacetten MultiLanguageDocTransformer MultiLanguageQueryParser ConditionalInsertField

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

testjar:
	jar cfm run.jar META-INF/MANIFEST.MF \
            -C RangedSearches/bin de/uni_tuebingen/ub/ixTheo/bibleRangeSearch/Range.class \
            -C RangedSearches/bin de/uni_tuebingen/ub/ixTheo/bibleRangeSearch/BibleRange.class
