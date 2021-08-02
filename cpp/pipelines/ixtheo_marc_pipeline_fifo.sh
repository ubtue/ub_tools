#!/bin/bash
# Runs through the phases of the IxTheo MARC processing pipeline.
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


StartPhase "Remove Dangling References"
(remove_dangling_references GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                            GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" \
                            dangling_references.log 2>&1 && \
EndPhase || Abort) &
wait
if [[ $(date +%d) == "01" ]]; then # Only do this on the 1st of every month.
    if [[ ! $(wc --lines dangling_references.log) =~ 0.* ]]; then
        echo "Sending email for dangling references..." | tee --append "${log}"
        send_email --priority=high \
                   --recipients=ixtheo-team@ub.uni-tuebingen.de \
                   --subject="Referenzen auf fehlende Datensätze gefunden ($(hostname))" \
                   --message-body="Liste im Anhang." \
                   --attachment="dangling_references.log"
    fi
fi


# WARNING; This phase needs to come early in the pipeline as it assumes that we have not yet
# added any of our own tags and because later phases may need to use the local data fields!
StartPhase "Add Local Data from Database"
(add_local_data GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Swap and Delete PPN's in Various Databases"
(patch_ppns_in_databases --report-only GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc Normdaten-"${date}".mrc \
                         -- entire_record_deletion.log >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Filter out Self-referential 856 Fields" \
           "\n\tRemove Sorting Chars From Title Subfields" \
           "\n\tRemove blmsh Subject Heading Terms" \
           "\n\tFix Local Keyword Capitalisations" \
           "\n\tStandardise German B.C. Year References"
(marc_filter \
     GesamtTiteldaten-post-phase"$((PHASE-2))"-"${date}".mrc GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
    --remove-fields '856u:ixtheo\.de' \
    --remove-fields 'LOK:086630(.*)\x{1F}x' `# Remove internal bibliographic comments` \
    --filter-chars 130a:240a:245a '@' \
    --remove-subfields '6002:blmsh' '6102:blmsh' '6302:blmsh' '6892:blmsh' '6502:blmsh' '6512:blmsh' '6552:blmsh' \
    --replace 600a:610a:630a:648a:650a:650x:651a:655a "(.*)\\.$" "\\1" `# Remove trailing periods for the following keyword normalisation.` \
    --replace-strings 600a:610a:630a:648a:650a:650x:651a:655a /usr/local/var/lib/tuelib/keyword_normalisation.map \
    --replace 100a:700a /usr/local/var/lib/tuelib/author_normalisation.map \
    --replace 260b:264b /usr/local/var/lib/tuelib/publisher_normalisation.map \
    --replace 245a "^L' (.*)" "L'\\1" `# Replace "L' arbe" with "L'arbe" etc.` \
    --replace 100a:700a "^\\s+(.*)" "\\1" `# Replace " van Blerk, Nico" with "van Blerk, Nico" etc.` \
    --replace 100d:689d:700d "v(\\d+)\\s?-\\s?v(\\d+)" "\\1 v.Chr.-\\2 v.Chr" \
    --replace 100d:689d:700d "v(\\d+)\\s?-\\s?(\\d+)" "\\1 v.Chr.-\\2" \
    --replace 100d:689d:700d "v(\\d+)" "\\1 v. Chr." \
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


StartPhase "Transfer 880 Authority Data Translations to 750"
(transfer_880_translations Normdaten-"${date}".mrc \
                           Normdaten-partially-augmented0-"${date}.mrc" \
                           >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Augment Authority Data with Keyword Translations"
(augment_authority_data_with_translations Normdaten-partially-augmented0-"${date}".mrc \
                                          Normdaten-partially-augmented1-"${date}".mrc \
                                          >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Add BEACON Information to Authority Data"
(add_authority_beacon_information Normdaten-partially-augmented1-"${date}".mrc \
                                  Normdaten-partially-augmented2-"${date}".mrc kalliope.staatsbibliothek-berlin.beacon \
                                  --type-file kalliope_originators.txt $(find . -name '*.beacon' ! -name "*kalliope.*") \
                                  >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Cross Link Articles"
(add_article_cross_links GesamtTiteldaten-post-phase"$((PHASE-5))"-"${date}".mrc \
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
make_named_pipe --buffer-size=$FIFO_BUFFER_SIZE GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
(add_superior_and_alertable_flags ixtheo GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                         GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &


# Note: It is necessary to run this phase after articles have had their journal's PPN's inserted!
StartPhase "Populate the Zeder Journal Timeliness Database Table"
(collect_journal_stats ixtheo GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                              GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


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

StartPhase "Add Wikidata IDs to Authority Data"
(add_authority_wikidata_ids Normdaten-partially-augmented2-"${date}".mrc \
                            Normdaten-partially-augmented3-"${date}".mrc \
                            /usr/local/var/lib/tuelib/gnd_to_wiki.txt >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait

StartPhase "Add Author Synonyms from Authority Data"
(add_author_synonyms GesamtTiteldaten-post-phase"$((PHASE-3))"-"${date}".mrc Normdaten-"${date}".mrc \
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
make_named_pipe --buffer-size=$FIFO_BUFFER_SIZE GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
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
make_named_pipe --buffer-size=$FIFO_BUFFER_SIZE GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
(augment_bible_references GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                         Normdaten-"${date}".mrc \
                         GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
cp pericopes_to_codes.map /usr/local/var/lib/tuelib/bibleRef/ && \
EndPhase || Abort) &


StartPhase "Augment Canon Law References"
make_named_pipe --buffer-size=$FIFO_BUFFER_SIZE GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
(augment_canones_references GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                            Normdaten-"${date}".mrc \
                            GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Augment Time Aspect References"
make_named_pipe --buffer-size=$FIFO_BUFFER_SIZE GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
(augment_time_aspects GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                      Normdaten-"${date}".mrc \
                      GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Update IxTheo Notations"
make_named_pipe --buffer-size=$FIFO_BUFFER_SIZE GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
(update_ixtheo_notations \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
    /usr/local/var/lib/tuelib/IxTheo_Notation.csv >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Replace 689\$A with 689\$q"
make_named_pipe --buffer-size=$FIFO_BUFFER_SIZE GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
(subfield_code_replacer GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                        GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                        "689A=q" >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Map DDC to IxTheo Notations"
make_named_pipe --buffer-size=$FIFO_BUFFER_SIZE GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
(map_ddc_to_ixtheo_notations \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
    /usr/local/var/lib/tuelib/ddc_ixtheo.map >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Add Keyword Synonyms from Authority Data"
(add_synonyms \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    Normdaten-partially-augmented3-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Fill in missing 773\$a Subfields"
make_named_pipe --buffer-size=$FIFO_BUFFER_SIZE GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
(augment_773a --verbose GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Tag further potential relbib entries"
make_named_pipe --buffer-size=$FIFO_BUFFER_SIZE GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
(add_additional_relbib_entries GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                               GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Integrate Reasonable Sort Year for Serials"
make_named_pipe --buffer-size=$FIFO_BUFFER_SIZE GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
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
make_named_pipe --buffer-size=$FIFO_BUFFER_SIZE GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
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
(add_subsystem_tags GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc Normdaten-partially-augmented3-"${date}".mrc \
                    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc Normdaten-partially-augmented4-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Appending Literary Remains Records"
make_named_pipe --buffer-size=$FIFO_BUFFER_SIZE GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
(create_literary_remains_records GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                 GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                                 Normdaten-partially-augmented4-"${date}".mrc \
                                 Normdaten-fully-augmented-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Add paragraph to CIC \$p"
make_named_pipe --buffer-size=$FIFO_BUFFER_SIZE GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1
(marc_augmentor GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                --replace-subfield-if-regex '610p:/(\d+)/§\1/' '610t:Codex iuris canonici' \
                --replace-subfield-if-regex '610p:/(\d+-\d+)/§\1/' '610t:Codex iuris canonici' \
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
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Tags Which Subsystems have Inferior Records in Superior Works Records"
(add_is_superior_work_for_subsystems GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
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
