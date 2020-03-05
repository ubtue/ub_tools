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
    echo -e "*** Phase $PHASE: $1 - $(date) ***" | tee --append "${log}"
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
    rm -f GesamtTiteldaten-post-phase*.mrc
}


# Sets up the log file:
logdir=/usr/local/var/log/tuefind
log="${logdir}/ixtheo_marc_pipeline_fifo.log"
rm -f "${log}"

CleanUp


OVERALL_START=$(date +%s.%N)


StartPhase "Check Record Integrity at the Beginning of the Pipeline"
(marc_check --do-not-abort-on-empty-subfields --do-not-abort-on-invalid-repeated-fields \
            --write-data=GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc GesamtTiteldaten-"${date}".mrc
    >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait
if [[ $(date +%d) == "01" ]]; then # Only do this on the 1st of every month.
    echo "*** Occasional Phase: Checking Rule Violations ***" | tee --append "${log}"
    marc_check_log="/usr/local/var/log/tuefind/marc_check_rule_violations.log"
    marc_check --check-rule-violations-only GesamtTiteldaten-"${date}".mrc \
               /usr/local/var/lib/tuelib/marc_check.rules "${marc_check_log}" >> "${log}" 2>&1 &&
    if [ -s "${marc_check_log}" ]; then
        send_email --priority=high \
                   --recipients=ixtheo-team@ub.uni-tuebingen.de \
                   --subject="marc_check Found Rule Violations" \
                   --message-body="PPNs im Anhang." \
                   --attachment="${marc_check_log}"
    fi
fi


StartPhase "Swap and Delete PPN's in Various Databases"
(patch_ppns_in_databases --report-only GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc Normdaten-"${date}".mrc \
                         -- entire_record_deletion.log >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Filter out Self-referential 856 Fields" \
           "\n\tRemove Sorting Chars From Title Subfields" \
           "\n\tRemove blmsh Subject Heading Terms" \
           "\n\tFix Local Keyword Capitalisations"
(marc_filter \
     GesamtTiteldaten-post-phase"$((PHASE-2))"-"${date}".mrc GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
    --remove-fields '856u:ixtheo\.de' \
    --remove-fields 'LOK:086630(.*)\x{1F}x' `# Remove internal bibliographic comments` \
    --filter-chars 130a:240a:245a '@' \
    --remove-subfields '6002:blmsh' '6102:blmsh' '6302:blmsh' '6892:blmsh' '6502:blmsh' '6512:blmsh' '6552:blmsh' \
    --replace-strings 600a:610a:630a:648a:650a:650x:651a:655a /usr/local/var/lib/tuelib/keyword_normalisation.map \
    --replace 100a:700a /usr/local/var/lib/tuelib/author_normalisation.map \
    --replace 260b:264b /usr/local/var/lib/tuelib/publisher_normalisation.map \
    --replace 245a "^L' (.*)" "L'\\1" `#  Replace "L' arbe" with "L'arbe" etc.` \
>> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Rewrite Authors and Standardized Keywords from Authority Data"
(rewrite_keywords_and_authors_from_authority_data GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                                  Normdaten-"${date}".mrc \
                                                  GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Add Missing Cross Links Between Records"
(add_missing_cross_links GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                         GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Extract Translations and Generate Interface Translation Files"
(extract_vufind_translations_for_translation \
    "$VUFIND_HOME"/local/tuefind/languages/de.ini `# German terms before all others.` \
    $(ls -1 "$VUFIND_HOME"/local/tuefind/languages/??.ini | grep -v 'de.ini$') >> "${log}" 2>&1 && \
generate_vufind_translation_files "$VUFIND_HOME"/local/tuefind/languages/ >> "${log}" 2>&1 && \
"$VUFIND_HOME"/clean_vufind_cache.sh >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Augment Authority Data with Keyword Translations"
(augment_authority_data_with_translations Normdaten-"${date}".mrc \
                                          Normdaten-partially-augmented1-"${date}".mrc \
                                          >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Add BEACON Information to Authority Data"
(add_authority_beacon_information Normdaten-partially-augmented1-"${date}".mrc \
                                  Normdaten-partially-augmented2-"${date}".mrc *.beacon >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Cross Link Articles"
(add_article_cross_links GesamtTiteldaten-post-phase"$((PHASE-4))"-"${date}".mrc \
                         GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                         article_matches.list >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Normalise URL's"
(normalise_urls GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Parent-to-Child Linking and Flagging of Subscribable Items"
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(add_superior_and_alertable_flags GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                  GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Add Additional Open Access URL's"
# Execute early for programs that rely on it for determining the open access property
OADOI_URLS_FILE="/mnt/ZE020150/FID-Entwicklung/oadoi/oadoi_urls_ixtheo.json"
(add_oa_urls ${OADOI_URLS_FILE} GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Extract Normdata Translations"
(extract_authority_data_translations Normdaten-partially-augmented2-"${date}".mrc \
                                     normdata_translations.txt >> "${log}" 2>&1 &&
EndPhase || Abort) &
wait


StartPhase "Add Author Synonyms from Authority Data"
(add_author_synonyms GesamtTiteldaten-post-phase"$((PHASE-2))"-"${date}".mrc Normdaten-"${date}".mrc \
                     GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Add ACO Fields to Records That Are Article Collections"
(flag_article_collections GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Adding of ISBN's and ISSN's to Component Parts"
(add_isbns_or_issns_to_articles GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Extracting Keywords from Titles"
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(enrich_keywords_with_title_words GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                 GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                                 /usr/local/var/lib/tuelib/stopwords.???  >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Flag Electronic and Open-Access Records"
(flag_electronic_and_open_access_records GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                         GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Augment Bible References"
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(augment_bible_references GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                         Normdaten-"${date}".mrc \
                         GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
cp pericopes_to_codes.map /usr/local/var/lib/tuelib/bibleRef/ && \
EndPhase || Abort) &


StartPhase "Augment Canon Law References"
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(augment_canones_references GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                            Normdaten-"${date}".mrc \
                            GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Augment Time Aspect References"
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(augment_time_aspects GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                      Normdaten-"${date}".mrc \
                      GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Update IxTheo Notations"
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(update_ixtheo_notations \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
    /usr/local/var/lib/tuelib/IxTheo_Notation.csv >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Replace 689\$A with 689\$q"
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(subfield_code_replacer GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                        GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                        "689A=q" >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Map DDC to IxTheo Notations"
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(map_ddc_to_ixtheo_notations \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
    /usr/local/var/lib/tuelib/ddc_ixtheo.map >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Add Keyword Synonyms from Authority Data"
(add_synonyms \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    Normdaten-partially-augmented2-"${date}".mrc \
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


StartPhase "Tag Records that are Available in Tübingen with an ITA Field"
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(flag_records_as_available_in_tuebingen \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Add Entries for Subscription Bundles and Tag Journals"
(add_subscription_bundle_entries GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                 GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Add Tags for subsystems"
(add_subsystem_tags GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc Normdaten-"${date}".mrc \
                    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Appending Literary Remains Records"
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(create_literary_remains_records GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                 GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                                 Normdaten-partially-augmented2-"${date}".mrc \
                                 Normdaten-fully-augmented-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Patching German BCE references"
mkfifo GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc
(marc_filter GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
             GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
             --globally-substitute 689d "v([0-9]+) ?- ?v([0-9]+)" "\\1 v. Chr. - \\2 v. Chr." `# Replace "v384-v322" with "384 v. Chr. - 322 v. Chr."` \
             --globally-substitute 689d 'v(\d+) ?- ?(\d+)' '\1 v.Chr.-\2' `# Replace v1-29 with 1 v.Chr-29` \
             --globally-substitute 689d "v([0-9]+)" "\\1 v. Chr." `# Replace "v384" with "384 v. Chr."` \
             --globally-substitute 109a "v([0-9]+) ?- ?v([0-9]+)" "\\1 v. Chr. - \\2 v. Chr." `# Replace "v384-v322" with "384 v. Chr. - 322 v. Chr."` \
             --globally-substitute 109a "v([0-9]+)" "\\1 v. Chr." `# Replace "v384" with "384 v. Chr."` \
             --globally-substitute 109a 'v(\d+) ?- ?(\d+)' '\1 v.Chr.-\2' `# Replace v1-29 with 1 v.Chr-29` \
             --globally-substitute SYGa 'v(\d+) ?- ?(\d+)' '\1 v.Chr.-\2' `# Replace v1-29 with 1 v.Chr-29` \
             --globally-substitute SYGa "v([0-9]+)" "\\1 v. Chr." `# Replace "v384" with "384 v. Chr."` \
             >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Tag PDA candidates"
# Use the most recent GVI PPN list.
(augment_pda \
    $(ls -t gvi_ppn_list-??????.txt | head -1) \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Patch Transitive Church Law, Religous Studies and Bible Studies Records"
(find_transitive_record_set GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                            GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc 2>&1 \
                            dangling_references.list && \
EndPhase || Abort) &
wait


StartPhase "Cross-link Type Tagging"
(add_cross_link_type GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-pipeline-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Check Record Integrity at the End of the Pipeline"
(marc_check --do-not-abort-on-empty-subfields --do-not-abort-on-invalid-repeated-fields \
            GesamtTiteldaten-post-pipeline-"${date}".mrc \
    >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Cleanup of Intermediate Files"
for p in $(seq 0 "$((PHASE-2))"); do
    rm -f GesamtTiteldaten-post-phase"$p"-??????.mrc
done
rm -f child_refs child_titles parent_refs
EndPhase

echo -e "\n\nPipeline done after $(CalculateTimeDifference $OVERALL_START $(date +%s.%N)) minutes." | tee --append "${log}"
echo "*** IXTHEO MARC PIPELINE DONE - $(date) ***" | tee --append "${log}"
