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


function StartPhase {
    if [ -z ${P+x} ]; then
        P=0
    else
        ((++P))
    fi
    START=$(date +%s.%N)
    echo "*** Phase $P: $1 - $(date) ***" | tee --append "${log}"
}


function EndPhase {
    PHASE_DURATION=$(echo "scale=2;($(date +%s.%N) - $START)/60" | bc --mathlib)
    echo "Done after ${PHASE_DURATION} minutes." | tee --append "${log}"
}


# Determines the embedded date of the files we're processing:
date=$(echo $(echo "$1" | cut -d- -f 2) | cut -d. -f1)

# Sets up the log file:
logdir=/var/log/ixtheo
log="${logdir}/ixtheo_marc_pipeline.log"
rm -f "${log}"


StartPhase "Convert MARC-21 to MARC-XML"
marc_grep GesamtTiteldaten-"${date}".mrc 'if "001" == ".*" extract *' marc_xml \
    > GesamtTiteldaten-post-phase"$P"-"${date}".xml 2>> "${log}"
marc_grep Normdaten-"${date}".mrc 'if "001" == ".*" extract *' marc_xml \
     > Normdaten-post-phase"$P"-"${date}".xml 2>> "${log}"
EndPhase


StartPhase "Filter out Local Data of Other Institutions"
delete_unused_local_data GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
                         GesamtTiteldaten-post-phase"$P"-"${date}".xml \
                         >> "${log}" 2>&1
EndPhase


StartPhase "Filter out Records Containing mtex in 935\$a"
marc_filter --drop GesamtTiteldaten-"${date}".xml GesamtTiteldaten-post-phase"$P"-"${date}".xml 935a:mtex \
    >> "${log}" 2>&1
EndPhase


StartPhase "Filter out Self-referential 856 Fields"
marc_filter --remove-fields GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
    GesamtTiteldaten-post-phase"$P"-"${date}".xml '856u:ixtheo\.de' >> "${log}" 2>&1
EndPhase


StartPhase "Extract Translation Keywords"
extract_keywords_for_translation GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
                                 Normdaten-post-phase0-"${date}".xml >> "${log}" 2>&1
extract_vufind_translations_for_translation \
    $(ls "$VUFIND_HOME"/local/languages/??.ini | grep 'de.ini$') \
    $(ls -1 "$VUFIND_HOME"/local/languages/??.ini | grep -v 'de.ini$') >> "${log}" 2>&1
EndPhase


StartPhase "Parent-to-Child Linking and Flagging of Subscribable Items"
create_superior_ppns.sh GesamtTiteldaten-post-phase"$((P-2))"-"${date}".xml >> "${log}" 2>&1
add_superior_and_alertable_flags GesamtTiteldaten-post-phase"$((P-2))"-"${date}".xml \
                                 GesamtTiteldaten-post-phase"$P"-"${date}".xml \
                                 superior_ppns >> "${log}" 2>&1
EndPhase


StartPhase "Add Author Synonyms from Authority Data"
add_author_synonyms GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml Normdaten-post-phase0-"${date}".xml \
                    GesamtTiteldaten-post-phase"$P"-"${date}".xml >> "${log}" 2>&1
EndPhase


StartPhase "Adding of ISBN's and ISSN's to Component Parts"
add_isbns_or_issns_to_articles GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
                               GesamtTiteldaten-post-phase"$P"-"${date}".xml >> "${log}" 2>&1
EndPhase


StartPhase "Extracting Keywords from Titles"
enrich_keywords_with_title_words GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
                                 GesamtTiteldaten-post-phase"$P"-"${date}".xml \
                                 ../cpp/data/stopwords.???
EndPhase


StartPhase "Augment Bible References"
augment_bible_references GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
                         Normdaten-post-phase0-"${date}".xml \
                         GesamtTiteldaten-post-phase"$P"-"${date}".xml >> "${log}" 2>&1
cp pericopes_to_codes.map /var/lib/tuelib/bibleRef/
EndPhase


StartPhase "Update IxTheo Notations"
update_ixtheo_notations \
    GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
    GesamtTiteldaten-post-phase"$P"-"${date}".xml \
    ../cpp/data/IxTheo_Notation.csv >> "${log}" 2>&1
EndPhase


StartPhase "Map DDC and RVK to IxTheo Notations"
map_ddc_and_rvk_to_ixtheo_notations \
    GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
    GesamtTiteldaten-post-phase"$P"-"${date}".xml \
    ../cpp/data/ddc_ixtheo.map ../cpp/data/ddc_ixtheo.map >> "${log}" 2>&1
EndPhase


StartPhase "Fill in missing 773\$a Subfields"
augment_773a --verbose GesamtTiteldaten-post-phase"$((P-1))"-"${date}".xml \
                       GesamtTiteldaten-post-pipeline-"${date}".xml >> "${log}" 2>&1
EndPhase


StartPhase "Extract Normdata Translations"
extract_normdata_translations Normdaten-post-phase0-"${date}".xml \
     normdata_translations.txt >> "${log}" 2>&1
EndPhase


StartPhase "Cleanup of Intermediate Files"
for p in $(seq 0 "$((P-1))"); do
    rm -f GesamtTiteldaten-post-phase"$p"-??????.xml
done
rm -r Normdaten-post-phase0-??????.xml
rm -f child_refs child_titles parent_refs
EndPhase


echo "*** IXTHEO MARC PIPELINE DONE - $(date) ***" | tee --append "${log}"
