#!/bin/bash
# Runs through the phases of the IxTheo MARC processing pipeline.
set -o errexit -o nounset

function ExitHandler {
    (setsid kill -- -$$) &
    exit 1
}

trap ExitHandler SIGINT

function Abort {
    kill -INT $$
}


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
    echo "Phase ${PHASE}: Done after ${PHASE_DURATION} minutes." | tee --append "${log}"
}


function CleanUp {
    rm -f GesamtTiteldaten-post-phase?*-"${date}".mrc
}


# Sets up the log file:
logdir=/var/log/ixtheo
log="${logdir}/ixtheo_marc_pipeline_fifo.log"
rm -f "${log}"

CleanUp

OVERALL_START=$(date +%s.%N)


StartPhase "Filter out Local Data of Other Institutions" 
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(delete_unused_local_data GesamtTiteldaten-"${date}".mrc \
                         GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                         >> "${log}" 2>&1 &&
EndPhase || Abort) &


StartPhase "Drop Records Containing mtex in 935, Filter out Self-referential 856 Fields, Superflous Subfield 2 in Topic Fields and Remove Sorting Chars\$a"
(marc_filter \
     GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
    --input-format=marc-21 \
    --output-format=marc-21 \
    --drop 935a:mtex \
    --remove-fields '856u:ixtheo\.de' \
    --filter-chars 130a:240a:245a '@' >> "${log}" \
    --remove-subfields '6002:blmsh' '6102:blmsh' '6302:blmsh' '6892:blmsh' '6502:blmsh' '6512:blmsh' '6552:blmsh' >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Extract Translation Keywords and Generate Interface Translation Files"
(extract_keywords_for_translation GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                 Normdaten-"${date}".mrc >> "${log}" 2>&1 && \
extract_vufind_translations_for_translation \
    "$VUFIND_HOME"/local/languages/de.ini \ # German terms before all others.
    $(ls -1 "$VUFIND_HOME"/local/languages/??.ini | grep -v 'de.ini$') >> "${log}" 2>&1 && \
generate_vufind_translation_files "$VUFIND_HOME"/local/languages/ >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait 


StartPhase "Normalise URL's"
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(normalise_urls GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait 


StartPhase "Parent-to-Child Linking and Flagging of Subscribable Items" 
(create_superior_ppns.sh GesamtTiteldaten-post-phase"$((PHASE-2))"-"${date}".mrc >> "${log}" 2>&1 && \
add_superior_and_alertable_flags GesamtTiteldaten-post-phase"$((PHASE-2))"-"${date}".mrc \
                                 GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                                 superior_ppns >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Add Author Synonyms from Authority Data" 
(add_author_synonyms GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc Normdaten-"${date}".mrc \
                    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Adding of ISBN's and ISSN's to Component Parts" 
(add_isbns_or_issns_to_articles GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                               GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait

StartPhase "Extracting Keywords from Titles" 
(enrich_keywords_with_title_words GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                 GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                                 ../cpp/data/stopwords.??? && \
EndPhase) &
wait


StartPhase "Augment Bible References" 
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(augment_bible_references GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                         Normdaten-"${date}".mrc \
                         GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
cp pericopes_to_codes.map /var/lib/tuelib/bibleRef/ && \
EndPhase || Abort) &


StartPhase "Update IxTheo Notations"
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(update_ixtheo_notations \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
    ../cpp/data/IxTheo_Notation.csv >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Map DDC and RVK to IxTheo Notations" 
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(map_ddc_and_rvk_to_ixtheo_notations \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
    ../cpp/data/ddc_ixtheo.map ../cpp/data/ddc_ixtheo.map >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Add Keyword Synonyms from Authority Data"
(add_synonyms \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    Normdaten-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Fill in missing 773\$a Subfields"
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(augment_773a --verbose GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Tag further potential relbib entries"
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(add_additional_relbib_entries GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Integrate Reasonable Sort Year for Serials"
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(add_publication_year_to_serials \
    Schriftenreihen-Sortierung-"${date}".txt \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Integrate Refterms"
(add_referenceterms Hinweissätze-Ergebnisse-"${date}".txt GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Adding the Library Sigil to Articles Where Appropriate"
(add_ub_sigil_to_articles --verbose \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait

StartPhase "Tag PDA candidates"
(augment_pda \
    gvi_ppn_list-"${date}".txt \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-pipeline-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Extract Normdata Translations"
(extract_normdata_translations Normdaten-"${date}".mrc \
     normdata_translations.txt >> "${log}" 2>&1 &&
EndPhase || Abort) &
wait 


StartPhase "Cleanup of Intermediate Files"
for p in $(seq 0 "$((PHASE-1))"); do
    rm -f GesamtTiteldaten-post-phase"$p"-??????.mrc
done
rm -f child_refs child_titles parent_refs
EndPhase

echo -e "\n\nPipeline done after $(CalculateTimeDifference $OVERALL_START $(date +%s.%N)) minutes." | tee --append "${log}"
echo "*** IXTHEO MARC PIPELINE DONE - $(date) ***" | tee --append "${log}"
