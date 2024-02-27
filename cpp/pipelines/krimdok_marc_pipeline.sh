#!/bin/bash
# Runs through the phases of the KrimDok MARC processing pipeline.
source pipeline_functions.sh
declare -r -i FIFO_BUFFER_SIZE=1000000 # in bytes

TMP_NULL="/tmp/null"
function CreateTemporaryNullDevice {
    if [ ! -c ${TMP_NULL} ]; then
        mknod ${TMP_NULL} c 1 3
        chmod 666 ${TMP_NULL}
    fi
}


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
CreateTemporaryNullDevice


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


StartPhase "Fix Local Keyword Capitalisations"
(marc_filter \
     GesamtTiteldaten-post-phase"$((PHASE-2))"-"${date}".mrc GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
     --replace-strings 600a:610a:630a:648a:650a:650x:651a:655a /usr/local/var/lib/tuelib/keyword_normalisation.map \
     >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Normalise URL's"
(normalise_urls GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
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
                    --store-pdfs-as-html --use-separate-entries-per-url --include-all-tocs \
                    --include-list-of-references --only-pdf-fulltexts \
                    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                    ${TMP_NULL} >> "${log}" 2>&1
EndPhase


StartPhase "Fill in the \"in_tuebingen_available\" Field"
krimdok_check_local_holdings --verbose \
                                GesamtTiteldaten-post-phase"$((PHASE-2))"-"${date}".mrc \
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
(add_superior_and_alertable_flags krimdok GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                          GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" \
    >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Add Additional Open Access URL's"
OADOI_URLS_FILE="/mnt/ZE020150/FID-Entwicklung/oadoi/oadoi_urls_krimdok.json"
(add_oa_urls ${OADOI_URLS_FILE} GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait

# Note: in krimdok this phase is used to count titles for each coorporation / author, no subsytems at the moment
StartPhase "Add Tags for subsystems"
(add_subsystem_tags krimdok GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc Normdaten-"${date}".mrc \
                    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc Normdaten-partially-augmented1-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Tags Which Subsystems have Inferior Records in Superior Works Records"
(add_is_superior_work_for_subsystems GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Add BEACON Information to Authority Data"
(add_authority_beacon_information Normdaten-partially-augmented1-"${date}".mrc \
                                  Normdaten-partially-augmented2-"${date}".mrc beacon_downloads/kalliope.staatsbibliothek-berlin.lr.beacon \
                                  --type-file kalliope_originators.txt $(find . -name '*.beacon' ! -name "*kalliope.*") \
                                  >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Add Wikidata IDs to Authority Data"
(add_authority_external_ref Normdaten-partially-augmented2-"${date}".mrc \
                            Normdaten-partially-augmented3-"${date}".mrc \
                            /usr/local/var/lib/tuelib/gnd_to_wiki.csv >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Appending Literary Remains Records"
(create_literary_remains_records --no-subsystems \
                                 GesamtTiteldaten-post-phase"$((PHASE-3))"-"${date}".mrc \
                                 GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                                 Normdaten-partially-augmented3-"${date}".mrc \
                                 Normdaten-fully-augmented-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Augment Time Aspect References"
(augment_time_aspects GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                      Normdaten-fully-augmented-"${date}".mrc \
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
rm -f Normdaten-partially-augmented?-??????.mrc
rm -f full_text.db
EndPhase


echo -e "\n\nPipeline done after $(CalculateTimeDifference $OVERALL_START $(date +%s.%N)) minutes." | tee --append "${log}"
echo "*** KRIMDOK MARC PIPELINE DONE ***" | tee --append "${log}"
