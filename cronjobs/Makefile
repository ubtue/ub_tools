PROGS=black_box_monitor.py  fetch_marc_updates.py  initiate_marc_pipeline.py purge_old_data.py delete_solr_ids.sh handle_partial_updates.py get_config_file_entry.py update_tad_email_acl.sh
LIBS=process_util.py util.py

all:

install:
	cp $(PROGS) $(LIBS) ~/bin/

root_install:
	cp $(PROGS) $(LIBS) /usr/local/bin/

clean:
	# Intentionally do nothing.
