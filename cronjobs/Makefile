PROGS=black_box_monitor.py  fetch_marc_updates.py  initiate_marc_pipeline.py purge_old_data.py handle_partial_updates.py get_config_file_entry.py update_tad_email_acl.sh create_refterm_file.py java_mem_monitor.sh fetch_interlibraryloan_ppns.py upload_crossref_records.py generate_beacon_file.py provide_ixtheo_translations.py collect_solr_stats_data.py update_oadoi_data.py initiate_fulltext_pipeline.py upload_to_bsz_ftp_server.py download_kalliope_originator_records.py extract_kalliope_originators.py generate_kalliope_originators.py add_authority_external_ref.py
LIBS=process_util.py util.py bsz_util.py ftp_connection.py
PYCACHE=/usr/local/bin/__pycache__/

all:

install:
	if [ -d $(PYCACHE) ]; then rm -r $(PYCACHE); fi
	cp $(PROGS) $(LIBS) /usr/local/bin/

clean:
	# Intentionally do nothing.
