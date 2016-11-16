#!/bin/bash
# Runs through the phases of the IxTheo MARC processing pipeline.
set -o errexit -o nounset

if [ -z "$VUFIND_HOME" ]; then
    VUFIND_HOME=/usr/local/vufind2
fi

if [ $# != 2 ]; then
    echo "usage: $0 GesamtTiteldaten-YYMMDD.mrc" "Normdaten-YYMMDD.mrc"
    exit 1
fi

if [[ ! "$1" =~ GesamtTiteldaten-[0-9][0-9][0-9][0-9][0-9][0-9].mrc ]]; then
    echo 'Die Gesamttiteldatendatei entspricht nicht dem Muster GesamtTiteldaten-[0-9][0-9][0-9][0-9][0-9][0-9].mrc!'
    exit 1
fi


# Determines the embedded date of the files we're processing:
date=$(echo $(echo "$1" | cut -d- -f 2) | cut -d. -f1)


if [[ "$2" != "Normdaten-${date}.mrc" ]]; then
    echo "Authority data file must be named \"$2\"\ to match the bibliographic file name!"
    exit 1
fi


function StartPhase {
    if [ -z ${PHASE+x} ]; then
        PHASE=0
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


# Sets up the log file:
logdir=/var/log/ixtheo
log="${logdir}/ixtheo_marc_pipeline.log"
rm -f "${log}"


OVERALL_START=$(date +%s.%N)


StartPhase "Filter out Local Data of Other Institutions"
delete_unused_local_data GesamtTiteldaten-"${date}".mrc \
                         GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                         >> "${log}" 2>&1
EndPhase


StartPhase "Drop Records Containing mtex in 935, Filter out Self-referential 856 Fields, and Remove Sorting Chars\$a"
marc_filter GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
            GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
    --drop 935a:mtex \
    --remove-fields '856u:ixtheo\.de' \
    --filter-chars 130a:240a:245a '@' \
    >> "${log}" 2>&1
EndPhase


StartPhase "Extract Translation Keywords and Generate Interface Translation Files"
extract_keywords_for_translation GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                 Normdaten-"${date}".mrc >> "${log}" 2>&1
extract_vufind_translations_for_translation \
    $(ls "$VUFIND_HOME"/local/languages/??.ini | grep 'de.ini$') \
    $(ls -1 "$VUFIND_HOME"/local/languages/??.ini | grep -v 'de.ini$') >> "${log}" 2>&1
generate_vufind_translation_files "$VUFIND_HOME"/local/languages/
EndPhase


StartPhase "Parent-to-Child Linking and Flagging of Subscribable Items"
create_superior_ppns.sh GesamtTiteldaten-post-phase"$((PHASE-2))"-"${date}".mrc >> "${log}" 2>&1
add_superior_and_alertable_flags GesamtTiteldaten-post-phase"$((PHASE-2))"-"${date}".mrc \
                                 GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                                 superior_ppns >> "${log}" 2>&1
EndPhase


StartPhase "Add Author Synonyms from Authority Data"
add_author_synonyms GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc Normdaten-"${date}".mrc \
                    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
EndPhase


StartPhase "Adding of ISBN's and ISSN's to Component Parts"
add_isbns_or_issns_to_articles GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                               GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
EndPhase


StartPhase "Extracting Keywords from Titles"
enrich_keywords_with_title_words GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                 GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                                 ../cpp/data/stopwords.???
EndPhase


StartPhase "Adding the Library Sigil to Articles Where Appropriate"
add_ub_sigil_to_articles GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                         GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
EndPhase


StartPhase "Augment Bible References"
augment_bible_references GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                         Normdaten-"${date}".mrc \
                         GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
cp pericopes_to_codes.map /var/lib/tuelib/bibleRef/
EndPhase


StartPhase "Update IxTheo Notations"
update_ixtheo_notations \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
    ../cpp/data/IxTheo_Notation.csv >> "${log}" 2>&1
EndPhase


StartPhase "Map DDC and RVK to IxTheo Notations"
map_ddc_and_rvk_to_ixtheo_notations \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
    ../cpp/data/ddc_ixtheo.map ../cpp/data/ddc_ixtheo.map >> "${log}" 2>&1
EndPhase


StartPhase "Add Keyword Synonyms"
add_synonyms \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    Normdaten-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
EndPhase


StartPhase "Fill in missing 773\$a Subfields"
augment_773a --verbose GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                       GesamtTiteldaten-post-pipeline-"${date}".mrc >> "${log}" 2>&1
EndPhase


StartPhase "Extract Normdata Translations"
extract_normdata_translations Normdaten-"${date}".mrc \
     normdata_translations.txt >> "${log}" 2>&1
EndPhase


StartPhase "Cleanup of Intermediate Files"
for p in $(seq "$((PHASE-1))"); do
    rm -f GesamtTiteldaten-post-phase"$p"-??????.mrc
done
rm -f child_refs child_titles parent_refs
EndPhase


echo -e "\n\nPipeline done after $(CalculateTimeDifference $OVERALL_START $(date +%s.%N)) minutes." | tee --append "${log}"
echo "*** IXTHEO MARC PIPELINE DONE - $(date) ***" | tee --append "${log}"
