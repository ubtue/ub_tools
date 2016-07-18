#!/bin/bash
# Runs through the phases of the IxTheo MARC processing pipeline.
set -o errexit -o nounset

if [ $# != 2 ]; then
    echo "usage: $0 GesamtTiteldaten-YYMMDD.mrc" "Normdaten-YYMMDD.mrc"
    exit 1
fi

if [[ ! "$1" =~ GesamtTiteldaten-[0-9][0-9][0-9][0-9][0-9][0-9].mrc ]]; then
    echo 'Die Gesamttiteldatendatei entspicht nicht dem Muster GesamtTiteldaten-[0-9][0-9][0-9][0-9][0-9][0-9].mrc!'
    exit 1
fi

# Determines the embedded date of the files we're processing:
date=$(echo $(echo "$1" | cut -d- -f 2) | cut -d. -f1)

# Sets up the log file:
logdir=/var/log/ixtheo
log="${logdir}/ixtheo_marc_pipeline.log"
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
echo "*** Phase $P: Filter out records containing mtex in 935\$a ***" | tee --append "${log}"
marc_filter --drop GesamtTiteldaten-"${date}".xml GesamtTiteldaten-post-phase"$P"-"${date}".xml 935a:mtex \
    >> "${log}" 2>&1
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Filter out local data of other Institutions ***" | tee --append "${log}"
delete_unused_local_data GesamtTiteldaten-"${date}".xml \
                         GesamtTiteldaten-post-phase"$P"-"${date}".xml \
                         >> "${log}" 2>&1
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Extract Translation Keywords - $(date) ***" | tee --append "${log}"
extract_keywords_for_translation GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
                                 Normdaten-"${date}".xml >> "${log}" 2>&1
extract_vufind_translations_for_translation \
    $(ls "$VUFIND_HOME"/local/languages/??.ini | grep 'de.ini$') \
    $(ls -1 "$VUFIND_HOME"/local/languages/??.ini | grep -v 'de.ini$') >> "${log}" 2>&1
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Parent-to-Child Linking - $(date) ***" | tee --append "${log}"
create_superior_ppns.sh GesamtTiteldaten-post-phase"$((P-2))"-"${date}".xml >> "${log}" 2>&1
add_superior_flag GesamtTiteldaten-post-phase"$((P-2))"-"${date}".xml \
                  GesamtTiteldaten-post-phase"$P"-"${date}".xml \
                  superior_ppns >> "${log}" 2>&1
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Add Author Synonyms from Norm Data ***" | tee --append "${log}"
add_author_synonyms GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml Normdaten-"${date}".xml \
                    GesamtTiteldaten-post-phase"$P"-"${date}".xml >> "${log}" 2>&1
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Adding of ISBN's and ISSN's to Component Parts - $(date) ***" | tee --append "${log}"
add_isbns_or_issns_to_articles GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
                               GesamtTiteldaten-post-phase"$P"-"${date}".xml >> "${log}" 2>&1
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Extracting Keywords from Titles - $(date) ***" | tee --append "${log}"
enrich_keywords_with_title_words GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
                                 GesamtTiteldaten-post-phase"$P"-"${date}".xml \
                                 ../cpp/data/stopwords.???
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Augment Bible References - $(date) ***" | tee --append "${log}"
augment_bible_references GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
                         Normdaten-"${date}".xml \
                         GesamtTiteldaten-post-phase"$P"-"${date}".xml >> "${log}" 2>&1
cp pericopes_to_codes.map /var/lib/tuelib/bibleRef/
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Update IxTheo Notations - $(date) ***" | tee --append "${log}"
update_ixtheo_notations \
    GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
    GesamtTiteldaten-post-phase"$P"-"${date}".xml \
    ../cpp/data/IxTheo_Notation.csv >> "${log}" 2>&1
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Map DDC and RVK to IxTheo Notations - $(date) ***" | tee --append "${log}"
map_ddc_and_rvk_to_ixtheo_notations \
    GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
    GesamtTiteldaten-post-phase"$P"-"${date}".xml \
    ../cpp/data/ddc_ixtheo.map ../cpp/data/ddc_ixtheo.map >> "${log}" 2>&1
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


((++P)); START=$(date +%s.%N)
echo "*** Phase $P: Fill in missing 773\$a Subfields ***" | tee --append "${log}"
augment_773a --verbose GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
                       GesamtTiteldaten-post-pipeline-"${date}".xml >> "${log}" 2>&1
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"

((++P)); START=$(date +%s.%N)
echo "*** Extract Normdata Translations" | tee --append "${log}"
extract_normdata_translations Normdaten-"${date}".xml \
     normdata_translations.txt >> "${log}" 2>&1
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"


START=$(date +%s.%N)
echo "*** Cleanup of Intermediate Files - $(date) ***" | tee --append "${log}"
for p in $(seq "$((P-1))"); do
    rm -f GesamtTiteldaten-post-phase"$p"-??????.xml
done
rm -f child_refs child_titles parent_refs
PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc -l)
echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"

echo "*** IXTHEO MARC PIPELINE DONE - $(date) ***" | tee --append "${log}"
