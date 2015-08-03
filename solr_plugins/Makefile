SUBDIRS = Schlagwortfolgensuche Bibelstellensuche

all: 
	for dir in $(SUBDIRS); do \
		echo ""; \
		echo "==============================="; \
		echo ""; \
		echo "MAKE " $$dir; \
		echo ""; \
		$(MAKE) -C $$dir; \
	done

install:
	for dir in $(SUBDIRS); do \
		echo ""; \
		echo "==============================="; \
		echo ""; \
		echo "MAKE " $$dir install;\
		echo ""; \
		$(MAKE) -C $$dir install;\
	done	

clean: 
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done
