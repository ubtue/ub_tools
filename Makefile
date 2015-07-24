PROGS=black_box_monitor.py  fetch_marc_updates.py  initiate_marc_pipeline.py
LIBS=process_util.py util.py


install: $(PROGS) $(LIBS)
	cp $^ /usr/local/bin/
