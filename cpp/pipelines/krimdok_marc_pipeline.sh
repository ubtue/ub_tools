#!/bin/bash
# Runs through the phases of the KrimDok MARC processing pipeline.
source pipeline_functions.sh
declare -r -i FIFO_BUFFER_SIZE=1000000 # in bytes


if [ $# != 1 ]; then
    echo "usage: $0 GesamtTiteldaten-YYMMDD.mrc"
    exit 1
fi

if [[ ! "$1" =~ GesamtTiteldaten-[0-9][0-9][0-9][0-9][0-9][0-9].mrc ]]; then
    echo 'Die Gesamttiteldatendatei entspricht nicht dem Muster GesamtTiteldaten-[0-9][0-9][0-9][0-9][0-9][0-9].mrc!'
    exit 1
fi

# Determines the embedded date of the files we're processing:
date=$(DetermineDateFromFilename $1)

# Set up the log file:
logdir=/usr/local/var/log/tuefind
log="${logdir}/krimdok_marc_pipeline.log"
rm -f "${log}"

CleanUp


OVERALL_START=$(date +%s.%N)


StartPhase "Check Record Integrity at the Beginning of the Pipeline"
(marc_check --do-not-abort-on-empty-subfields --do-not-abort-on-invalid-repeated-fields \
            --write-data=GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc GesamtTiteldaten-"${date}".mrc \
    >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


# WARNING; This phase needs to come first in the pipeline as it assumes that we have not yet
# added any of our own tags and because later phases may need to use the local data fields!
StartPhase "Add Local Data from Database"
(add_local_data GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait

StartPhase "Replace old BSZ PPN's with new K10+ PPN's"
(patch_ppns_in_databases --report-only GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc Normdaten-"${date}".mrc \
                         -- entire_record_deletion.log >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Normalise URL's"
(normalise_urls GesamtTiteldaten-post-phase"$((PHASE-2))"-"${date}".mrc \
                GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" \
    >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Add Author Synonyms from Authority Data"
add_author_synonyms GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc Normdaten-"${date}".mrc \
                    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
EndPhase


StartPhase "Add PDA Fields to Some Records"
krimdok_flag_pda_records 3 \
                         GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                         GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
EndPhase


StartPhase "Flag Electronic and Open-Access Records"
flag_electronic_and_open_access_records GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                        GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
EndPhase


StartPhase "Add ISBN's or ISSN's to Articles"
add_isbns_or_issns_to_articles GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                               GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
EndPhase


StartPhase "Create Full-Text Database"
create_full_text_db --process-count-low-and-high-watermarks \
                    $(get_config_file_entry.py krimdok_marc_pipeline.conf \
                    create_full_text_db process_count_low_and_high_watermarks) \
                    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
EndPhase


StartPhase "Fill in the \"in_tuebingen_available\" Field"
populate_in_tuebingen_available --verbose \
                                GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
EndPhase


StartPhase "Fill in missing 773\$a Subfields"
augment_773a --verbose GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                       GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
EndPhase


StartPhase "Integrate Reasonable Sort Year for Serials"
add_publication_year_to_serials \
    Schriftenreihen-Sortierung-"${date}".txt \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
EndPhase


StartPhase "Parent-to-Child Linking and Flagging of Subscribable Items"
make_named_pipe --buffer-size=$FIFO_BUFFER_SIZE GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(add_superior_and_alertable_flags GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                  GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" \
    >> "${log}" 2>&1 && \
EndPhase || Abort) &


# Note: It is necessary to run this phase after articles have had their journal's PPN's inserted!
StartPhase "Populate the Zeder Journal Timeliness Database Table"
make_named_pipe --buffer-size=$FIFO_BUFFER_SIZE GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(collect_journal_stats krimdok GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                               GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Add Additional Open Access URL's"
OADOI_URLS_FILE="/mnt/ZE020150/FID-Entwicklung/oadoi/oadoi_urls_krimdok.json"
(add_oa_urls ${OADOI_URLS_FILE} GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Tags Which Subsystems have Inferior Records in Superior Works Records"
(add_is_superior_work_for_subsystems GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Check Record Integity at the End of the Pipeline"
(marc_check --do-not-abort-on-empty-subfields --do-not-abort-on-invalid-repeated-fields \
            --write-data=GesamtTiteldaten-post-pipeline-"${date}".mrc GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Cleanup of Intermediate Files"
for p in $(seq "$((PHASE-1))"); do
    rm -f GesamtTiteldaten-post-phase"$p"-??????.mrc
done
rm -f full_text.db
EndPhase


echo -e "\n\nPipeline done after $(CalculateTimeDifference $OVERALL_START $(date +%s.%N)) minutes." | tee --append "${log}"
echo "*** KRIMDOK MARC PIPELINE DONE ***" | tee --append "${log}"
