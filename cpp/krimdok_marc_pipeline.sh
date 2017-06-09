#!/bin/bash
# Runs through the phases of the KrimDok MARC processing pipeline.
set -o errexit -o nounset

if [ -z "$VUFIND_HOME" ]; then
    VUFIND_HOME=/usr/local/vufind
fi

if [ $# != 1 ]; then
    echo "usage: $0 GesamtTiteldaten-YYMMDD.mrc"
    exit 1
fi

if [[ ! "$1" =~ GesamtTiteldaten-[0-9][0-9][0-9][0-9][0-9][0-9].mrc ]]; then
    echo 'Die Gesamttiteldatendatei entspricht nicht dem Muster GesamtTiteldaten-[0-9][0-9][0-9][0-9][0-9][0-9].mrc!'
    exit 1
fi


# Determines the embedded date of the files we're processing:
date=$(echo $(echo "$1" | cut -d- -f 2) | cut -d. -f1)


function StartPhase {
    if [ -z ${PHASE+x} ]; then
        PHASE=1
    else
        ((++PHASE))
    fi
    START=$(date +%s.%N)
    echo "*** Phase $PHASE: $1 - $(date) ***" | tee --append "${log}"
}


# Call with "CalculateTimeDifference $start $end".
# $start and $end have to be in seconds.
# Returns the difference in fractional minutes as a string.
function CalculateTimeDifference {
    start=$1
    end=$2
    echo "scale=2;($end - $start)/60" | bc --mathlib
}


function EndPhase {
    PHASE_DURATION=$(CalculateTimeDifference $START $(date +%s.%N))
    echo -e "Done after ${PHASE_DURATION} minutes.\n" | tee --append "${log}"
}


# Set up the log file:
logdir=/var/log/krimdok
log="${logdir}/krimdok_marc_pipeline.log"
rm -f "${log}"


OVERALL_START=$(date +%s.%N)


StartPhase "Apply Updates to Our Authority Data"
update_authority_data 'LOEPPN-\d\d\d\d\d\d' 'Normdaten-\d\d\d\d\d\d'.mrc 'WA-MARCcomb-\d\d\d\d\d\d.tar.gz' Normdaten-"${date}".mrc >> "${log}" 2>&1 &&
EndPhase


StartPhase "Filter out Records of Other Institutions"
delete_unused_local_data GesamtTiteldaten-"${date}".mrc \
                         GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                         >> "${log}" 2>&1
EndPhase


StartPhase "Normalise URL's"
normalise_urls GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
               GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
EndPhase


StartPhase "Add Author Synonyms from Norm Data"
add_author_synonyms GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc Normdaten-"${date}".mrc \
                    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
EndPhase


StartPhase "Add PDA Fields to Some Records"
krimdok_flag_pda_records 3 \
                         GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                         GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
EndPhase


StartPhase "Add ISBN's or ISSN's to Articles"
add_isbns_or_issns_to_articles GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                               GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
EndPhase


StartPhase "Create Full-Text Database"
mkdir --parent fulltext/
create_full_text_db --process-count-low-and-high-watermarks \
                    $(get_config_file_entry.py krimdok_marc_pipeline.conf \
                    create_full_text_db process_count_low_and_high_watermarks) \
                    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                    full_text.db >> "${log}" 2>&1

cat fulltext/* >> GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
rm -R fulltext/
cp full_text.db /var/lib/tuelib/
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


StartPhase "Parent-to-Child Linking and Flagging of Subscribable Items"
create_superior_ppns.sh GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc >> "${log}" 2>&1 && \
add_superior_and_alertable_flags GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                 GesamtTiteldaten-post-pipeline-"${date}".mrc \
                                 superior_ppns >> "${log}" 2>&1
EndPhase


StartPhase "Cleanup of Intermediate Files"
for p in $(seq "$((PHASE-1))"); do
    rm -f GesamtTiteldaten-post-phase"$PHASE"-??????.mrc
done
rm -f full_text.db
EndPhase


echo -e "\n\nPipeline done after $(CalculateTimeDifference $OVERALL_START $(date +%s.%N)) minutes." | tee --append "${log}"
echo "*** KRIMDOK MARC PIPELINE DONE ***" | tee --append "${log}"
