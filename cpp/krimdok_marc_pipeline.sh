#!/bin/bash
# Runs through the phases of the KrimDok MARC processing pipeline.
set -o errexit -o nounset

if [ $# != 2 ]; then
    echo "usage: $0 GesamtTiteldaten-YYMMDD.mrc" \
         "Normdaten-YYMMDD.mrc"
    exit 1
fi

if [[ ! "$1" =~ GesamtTiteldaten-[0-9][0-9][0-9][0-9][0-9][0-9].mrc ]]; then
    echo 'Die Gesamttiteldatendatei entspicht nicht dem Muster GesamtTiteldaten-[0-9][0-9][0-9][0-9][0-9][0-9].mrc!'
    exit 1
fi

# Extract date:
date=$(echo $(echo "$1" | cut -d- -f 2) | cut -d. -f1)

# Set up the log file:
logdir=/var/log/krimdok
log="${logdir}/krimdok_marc_pipeline.log"
rm -f "${log}"

P=0; START=$(date +%s.%N)
echo "*** Phase $P: Convert MARC-21 to MARC-XML ***" | tee --append "${log}"
marc_grep GesamtTiteldaten-"${date}".mrc 'if "001" == ".*" extract *' marc_xml \
    > GesamtTiteldaten-"${date}".xml 2>> "${log}"
marc_grep Normdaten-"${date}".mrc 'if "001" == ".*" extract *' marc_xml \
    > Normdaten-"${date}".xml 2>> "${log}"
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Filter out Records of Other Institutions ***" | tee --append "${log}"
marc_filter --bibliotheks-sigel-filtern GesamtTiteldaten-"${date}".xml \
                                        GesamtTiteldaten-post-phase"$P"-"${date}".xml \
                                        >> "${log}" 2>&1
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Normalise URL's ***" | tee --append "${log}"
marc_filter --normalise-urls GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
                             GesamtTiteldaten-post-phase"$P"-"${date}".xml >> "${log}" 2>&1
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Add PDA Fields to Some Records ***" | tee --append "${log}"
krimdok_flag_pda_records GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
                         GesamtTiteldaten-post-phase"$P"-"${date}".xml >> "${log}" 2>&1
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Add Author Synonyms from Norm Data ***" | tee --append "${log}"
add_author_synonyms GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml Normdaten-"${date}".xml \
                    GesamtTiteldaten-post-phase"$P"-"${date}".xml >> "${log}" 2>&1
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Add ISBN's or ISSN's to Articles ***" | tee --append "${log}"
add_isbns_or_issns_to_articles GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
                               GesamtTiteldaten-post-phase"$P"-"${date}".xml >> "${log}" 2>&1
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Create Full-Text Database ***" | tee --append "${log}"
create_full_text_db --process-count-low-and-high-watermarks \
                    $(get_config_file_entry.py krimdok_marc_pipeline.conf \
                      create_full_text_db process_count_low_and_high_watermarks) \
                    GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
                    GesamtTiteldaten-post-phase"$P"-"${date}".xml \
                    full_text.db >> "${log}" 2>&1
cp full_text.db /var/lib/tuelib/
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Fill in the \"in_tuebingen_available\" Field ***" | tee --append "${log}"
populate_in_tuebingen_available --verbose \
                                GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
                                GesamtTiteldaten-post-phase"$P"-"${date}".xml >> "${log}" 2>&1
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Fill in missing 773\$a Subfields ***" | tee --append "${log}"
augment_773a --verbose GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
                       GesamtTiteldaten-post-pipeline-"${date}".xml >> "${log}" 2>&1
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


START=$(date +%s.%N)
echo "*** Cleanup of Intermediate Files ***" | tee --append "${log}"
for p in $(seq "$((P-1))"); do
    rm -f GesamtTiteldaten-post-phase"$p"-??????.xml
done
rm -f full_text.db
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


echo "*** KRIMDOK MARC PIPELINE DONE ***" | tee --append "${log}"
