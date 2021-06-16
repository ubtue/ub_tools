#!/bin/python3
# -*- coding: utf-8 -*-

# Algorithm to bring pipeline phases in an ideal order
# change from python 3.9 graphlib to toposort because it 
# not only provides a flatten order but also a order with alternatives

import argparse
from toposort import toposort
import itertools as it

# possible entries for title_norm field:
#   t = title data
#   n = norm (authority) data
#   x = has predecessor in title data AND norm data
#       no fifo for input possible (because reading from mult. files)
#       also used for first phase (integrity check)
#   x_t = same as x but output fifo for t possible 
#   x_n = same as x but output fifo for n possible 
#
# convention in dict-keys: 
#   leading "_" can only run after predecessor has finished
#   trailing "_" successor must not run before job has finished
#   leading and trailing "_" has to run independently
#
# restriction:
#   only one valid order is processed.
#   there is no comparision between different valid orders
#   so if you want to put a phase to the front you have to add dependencies
#

class Phase:
    key = "" 
    mode = ""
    title_norm = ""
    name = ""
    command = ""
    def __init__(self, _key, _mode, _title_norm, _name):
        self.key = _key
        self.mode = _mode
        self.title_norm = _title_norm
        self.name = _name



phase_counter = 0
fifo_counter = 0

phases = [
        Phase("check_record_integrity_beginning","_i","t","Check Record Integrity at the Beginning of the Pipeline"),
        Phase("remove_dangling_references","_i","t","Remove Dangling References [rewind]"),
        Phase("add_local_data_from_database","","t","Add Local Data from Database"),
        Phase("swap_and_delete_ppns","_i_","x","Swap and Delete PPN's in Various Databases [used in merge_differential_and_full_marc_updates, maybe independent]"),
        Phase("filter_856_etc","","t","Filter out Self-referential 856 Fields AND Remove Sorting Chars From Title Subfields AND Remove blmsh Subject Heading Terms AND Fix Local Keyword Capitalisations AND Standardise German B.C. Year References"),
        Phase("rewrite_authors_from_authority_data","","t","Rewrite Authors and Standardized Keywords from Authority Data"),
        Phase("add_missing_cross_links","_i","t","Add Missing Cross Links Between Records [rewind]"),
        Phase("extract_translations","_i_","x","Extract Translations and Generate Interface Translation Files [from vufind ini files to db and vice versa, maybe independent]"),
        Phase("transfer_880_to_750","","n","Transfer 880 Authority Data Translations to 750"),
        Phase("augment_authority_data_with_keyword_translations","","n","Augment Authority Data with Keyword Translations"),
        Phase("add_beacon_to_authority_data","","n","Add BEACON Information to Authority Data"),
        Phase("cross_link_articles","_i","t","Cross Link Articles [rewind]"),
        Phase("normalize_urls","","t","Normalise URL's"),
        Phase("parent_to_child_linking","_i","t","Parent-to-Child Linking and Flagging of Subscribable Items [rewind]"),
        Phase("populate_zeder_journal","","t","Populate the Zeder Journal Timeliness Database Table"),
        Phase("add_additional_open_access","","t","Add Additional Open Access URL's"),
        Phase("extract_normdata_translations","_i_","n","Extract Normdata Translations"),
        Phase("add_author_synomyns_from_authority","","t","Add Author Synonyms from Authority Data"),
        Phase("add_aco_fields","_i","t","Add ACO Fields to Records That Are Article Collections [rewind]"),
        Phase("add_isbn_issn","_i","t","Adding of ISBN's and ISSN's to Component Parts [rewind]"),
        Phase("extract_keywords_from_titles","_i","t","Extracting Keywords from Titles [rewind]"),
        Phase("flag_electronic_and_open_access","","t","Flag Electronic and Open-Access Records"),
        Phase("augment_bible_references","","t","Augment Bible References"),
        Phase("augment_canon_law_references","","t","Augment Canon Law References"),
        Phase("augment_time_aspect_references","","t","Augment Time Aspect References"),
        Phase("update_ixtheo_notations","","t","Update IxTheo Notations"),
        Phase("replace_689a_689q","","t","Replace 689\$A with 689\$q"),
        Phase("map_ddc_to_ixtheo_notations","","t","Map DDC to IxTheo Notations [reads csv file ddc_ixtheo.map - what source?]"),
        Phase("add_keyword_synonyms_from_authority","_i","x_t","Add Keyword Synonyms from Authority Data"),
        Phase("fill_missing_773a","_i","t","Fill in missing 773\$a Subfields [rewind]"),
        Phase("tag_further_potential_relbib_entries","","t","Tag further potential relbib entries"),
        Phase("integrate_sort_year_for_serials","","t","Integrate Reasonable Sort Year for Serials"),
        Phase("integrate_refterms","","t","Integrate Refterms"),
        Phase("tag_tue_records_with_ita_field","_i","t","Tag Records that are Available in Tübingen with an ITA Field [rewind]"),
        Phase("add_entries_for_subscription_bundles","","t","Add Entries for Subscription Bundles and Tag Journals"),
        Phase("add_tags_for_subsystems","_i","t","Add Tags for subsystems [rewind]"),
        Phase("appending_literary_remains","_i","x_t","Appending Literary Remains Records"),
        Phase("tag_pda_candidates","","t","Tag PDA candidates"),
        Phase("patch_transitive_records","_i","t","Patch Transitive Church Law, Religous Studies and Bible Studies Records [rewind]"),
        Phase("cross_link_type_tagging","_i","t","Cross-link Type Tagging [rewind]"),
        Phase("tag_inferior_records","_i","t","Tags Which Subsystems have Inferior Records in Superior Works Records [rewind]"),
        Phase("check_record_integrity_end","_i_","t","Check Record Integrity at the End of the Pipeline [does not produce new mrc file, i.e. fifo not usable]"),
        Phase("cleanup","_i_","x","Cleanup of Intermediate Files")
        ]

# every phase should be part of the dependency graph (key of phase)
graph = {
        "remove_dangling_references" : {"check_record_integrity_beginning"},
        "add_local_data_from_database" : {"remove_dangling_references"},
        "swap_and_delete_ppns" : {"check_record_integrity_beginning"},
        "filter_856_etc" : {"add_local_data_from_database"},
        "rewrite_authors_from_authority_data" : {"filter_856_etc"},
        "add_missing_cross_links" : {"rewrite_authors_from_authority_data"},
        "extract_translations" : {"check_record_integrity_beginning"},
        "transfer_880_to_750" : {"check_record_integrity_beginning"},
        "augment_authority_data_with_keyword_translations" : {"transfer_880_to_750"},
        "add_beacon_to_authority_data" : {"augment_authority_data_with_keyword_translations"},
        "cross_link_articles" : {"add_missing_cross_links"},
        "normalize_urls" : {"swap_and_delete_ppns"},
        "parent_to_child_linking" : {"normalize_urls"},
        "populate_zeder_journal" : {"parent_to_child_linking"},
        "add_additional_open_access" : {"populate_zeder_journal"},
        "extract_normdata_translations" : {"add_beacon_to_authority_data"},
        "add_author_synomyns_from_authority" : {"add_additional_open_access"},
        "add_aco_fields" : {"add_author_synomyns_from_authority"},
        "add_isbn_issn" : {"add_aco_fields"},
        "extract_keywords_from_titles" : {"add_isbn_issn"},
        "flag_electronic_and_open_access" : {"extract_keywords_from_titles"},
        "augment_bible_references" : {"flag_electronic_and_open_access"},
        "augment_canon_law_references" : {"augment_bible_references"},
        "augment_time_aspect_references" : {"augment_canon_law_references"},
        "update_ixtheo_notations" : {"augment_time_aspect_references"},
        "replace_689a_689q" : {"update_ixtheo_notations"},
        "map_ddc_to_ixtheo_notations" : {"replace_689a_689q"},
        "add_keyword_synonyms_from_authority" : {"add_beacon_to_authority_data", "map_ddc_to_ixtheo_notations"},
        "fill_missing_773a" : {"add_keyword_synonyms_from_authority"},
        "tag_further_potential_relbib_entries" : {"fill_missing_773a"},
        "integrate_sort_year_for_serials" : {"tag_further_potential_relbib_entries"},
        "integrate_refterms" : {"integrate_sort_year_for_serials"},
        "tag_tue_records_with_ita_field" : {"integrate_refterms"},
        "add_entries_for_subscription_bundles" : {"tag_tue_records_with_ita_field"},
        "add_tags_for_subsystems" : {"add_entries_for_subscription_bundles"},
        "appending_literary_remains" : {"add_beacon_to_authority_data", "add_tags_for_subsystems"},
        "tag_pda_candidates" : {"add_tags_for_subsystems"},
        "patch_transitive_records" : {"tag_pda_candidates"},
        "cross_link_type_tagging" : {"patch_transitive_records"},
        "tag_inferior_records" : {"cross_link_type_tagging"},
        "check_record_integrity_end" : {"tag_inferior_records"},
        "cleanup" : {"check_record_integrity_end"}
       }

def get_phase_by_key(key):
    for phase in phases:
        if phase.key == key:
            return phase
    return None

def process_chunk(lst, do_print = False):
    global phase_counter
    global fifo_counter
    if len(lst) > 0:
        for i, elem in enumerate(lst):
            phase_counter += 1
            if len(lst) > 1 and i != len(lst) - 1:
                fifo_counter += 1
                if do_print == True:
                    print("making fifo - ", elem.title_norm, " phase counter: ", phase_counter)
            if do_print == True:
                print("processing ", elem.key, " -> ", elem.title_norm, " phase counter: ", phase_counter, "[", elem.mode, "]")
        if do_print == True:
            print("---")

def split_and_write_pipeline_steps(lst, n, do_print = False):
    global phase_counter
    global fifo_counter
    phase_counter = 0
    fifo_counter = 0

    open_fifo_title = []
    open_fifo_norm =[] 

    for elemkey in lst:
        elem = get_phase_by_key(elemkey)
        phase_mode = elem.mode
        phase_title_norm = elem.title_norm

        if phase_title_norm.startswith("x"):
            process_chunk(open_fifo_title, do_print)
            process_chunk(open_fifo_norm, do_print)
            open_fifo_title = []
            open_fifo_norm = [] 
        elif phase_mode.startswith("_"):
            if phase_title_norm.startswith("t"):
                process_chunk(open_fifo_title, do_print)
                open_fifo_title = []
            if phase_title_norm.startswith("n"):
                process_chunk(open_fifo_norm, do_print)
                open_fifo_norm = []

        if len(open_fifo_title) == n:
            process_chunk(open_fifo_title, do_print)
            open_fifo_title = []
        if len(open_fifo_norm) == n:
            process_chunk(open_fifo_norm, do_print)
            open_fifo_norm = []

        if phase_title_norm.startswith("t"):
            open_fifo_title.append(elem)
        elif phase_title_norm.startswith("n"):
            open_fifo_norm.append(elem)
        elif phase_title_norm.startswith("x"):
            if phase_title_norm == "x_t":
                open_fifo_title.append(elem)
            elif phase_title_norm == "x_n":
                open_fifo_norm.append(elem)
            else:
                process_chunk([elem], do_print)

        if phase_mode.endswith("_") or elemkey == lst[-1]:
            if phase_title_norm.startswith("t") or phase_title_norm == "x_t":
                process_chunk(open_fifo_title, do_print)
                open_fifo_title = []
            elif phase_title_norm.startswith("n") or phase_title_norm == "x_n":
                process_chunk(open_fifo_norm, do_print)
                open_fifo_norm = []


def Main():
    parser = argparse.ArgumentParser(description='bring pipeline phases in an ideal order')
    parser.add_argument("--max-group-size", default=5, type=int, help="(max.) number of elements inside a running group")
    parser.add_argument("--use-flatten", default=0, type=int, help="use only one possible order instead of all possible variants")
    args = parser.parse_args()

    is_debug = False
    max_group_size = args.max_group_size
    use_flatten = args.use_flatten

    print("")
    # Checking if all phases used in deps-graph are defined
    for elem in graph:
        if not any(x.key == elem for x in phases):
            print(elem, " used in dependency: ", elem, " -> ",  graph[elem], " but not defined in phases")
            exit()
        for subelem in graph[elem]:
            if not any(x.key == subelem for x in phases):
                print(subelem, " used in dependency: ", elem, " -> ",  graph[elem], " but not defined in phases")
                exit()

    # Determine dependencies
    if is_debug:
        print("\nDependencies: ", graph)
        
    global fifo_counter
    best_fifo = 0
    best_order = []

    order_topo = list(toposort(graph))

    #c = [{1,2,3},{4,5}]  a = {1,2,3}   b = {4,5}
    #print(list(it.product(it.permutations(a), it.permutations(b))))
    #print(list(it.product(*(it.permutations(k) for k in c))))
    if use_flatten == False:
        all_combinations = list(it.product(*(it.permutations(k) for k in order_topo)))
        print("There are: ", len(all_combinations), " possible combinations")
        for tup_elem in all_combinations:
         elem = list(it.chain(*tup_elem))
         split_and_write_pipeline_steps(elem, max_group_size, False) #sets global var. fifo_counter
         if fifo_counter > best_fifo:
             best_fifo = fifo_counter
             best_order = elem
    else:
        best_order = list(it.chain(*order_topo))

    split_and_write_pipeline_steps(best_order, max_group_size, True)
    print("Created: ", fifo_counter, " fifos")
    print("")



#outside Main

phase_iter = get_phase_by_key("check_record_integrity_beginning")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(marc_check --do-not-abort-on-empty-subfields --do-not-abort-on-invalid-repeated-fields \
            --write-data=GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc GesamtTiteldaten-"${date}".mrc
    >> "${log}" 2>&1 && \
EndPhase || Abort) &

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

"""

phase_iter = get_phase_by_key("remove_dangling_references")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
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
                   --subject="Referenzen auf fehlende Datensätze gefunden" \
                   --message-body="Liste im Anhang." \
                   --attachment="dangling_references.log"
    fi
fi
"""

phase_iter = get_phase_by_key("add_local_data_from_database")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(add_local_data GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("swap_and_delete_ppns")
phase_iter.command = \
    "StartPhase ", phase_iter.name, "\\",
r"""
(patch_ppns_in_databases --report-only GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc Normdaten-"${date}".mrc \
                         -- entire_record_deletion.log >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("filter_856_etc")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
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
    --replace 100a:700a "^\\s+(.*)" "\\\\1" `# Replace " van Blerk, Nico" with "van Blerk, Nico" etc.` \
    --replace 100d:689d:700d "v(\\d+)\\s?-\\s?v(\\d+)" "\\1 v.Chr.-\\2 v.Chr" \
    --replace 100d:689d:700d "v(\\d+)\\s?-\\s?(\\d+)" "\\1 v.Chr.-\\2" \
    --replace 100d:689d:700d "v(\\d+)" "\\1 v. Chr." \
>> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("rewrite_authors_from_authority_data")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(rewrite_keywords_and_authors_from_authority_data GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                                  Normdaten-"${date}".mrc \
                                                  GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("add_missing_cross_links")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(augment_authority_data_with_translations Normdaten-partially-augmented0-"${date}".mrc \
                                          Normdaten-partially-augmented1-"${date}".mrc \
                                          >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("extract_translations")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(extract_vufind_translations_for_translation \
    "$VUFIND_HOME"/local/tuefind/languages/de.ini `# German terms before all others.` \
    $(ls -1 "$VUFIND_HOME"/local/tuefind/languages/??.ini | grep -v 'de.ini$') >> "${log}" 2>&1 && \
generate_vufind_translation_files "$VUFIND_HOME"/local/tuefind/languages/ >> "${log}" 2>&1 && \
"$VUFIND_HOME"/clean_vufind_cache.sh >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("transfer_880_to_750")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(transfer_880_translations Normdaten-"${date}".mrc \
                           Normdaten-partially-augmented0-"${date}.mrc" \
                           >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("augment_authority_data_with_keyword_translations")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(augment_authority_data_with_translations Normdaten-partially-augmented0-"${date}".mrc \
                                          Normdaten-partially-augmented1-"${date}".mrc \
                                          >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("add_beacon_to_authority_data")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(add_authority_beacon_information Normdaten-partially-augmented1-"${date}".mrc \
                                  Normdaten-partially-augmented2-"${date}".mrc *.beacon >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("cross_link_articles")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(add_article_cross_links GesamtTiteldaten-post-phase"$((PHASE-5))"-"${date}".mrc \
                         GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                         article_matches.list >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("normalize_urls")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(normalise_urls GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("parent_to_child_linking")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(add_superior_and_alertable_flags GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                  GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("populate_zeder_journal")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(collect_journal_stats ixtheo GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                              GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("add_additional_open_access")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
# Execute early for programs that rely on it for determining the open access property
OADOI_URLS_FILE="/mnt/ZE020150/FID-Entwicklung/oadoi/oadoi_urls_ixtheo.json"
(add_oa_urls ${OADOI_URLS_FILE} GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("extract_normdata_translations")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(extract_authority_data_translations Normdaten-partially-augmented2-"${date}".mrc \
                                     normdata_translations.txt >> "${log}" 2>&1 &&
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("add_author_synomyns_from_authority")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(add_author_synonyms GesamtTiteldaten-post-phase"$((PHASE-2))"-"${date}".mrc Normdaten-"${date}".mrc \
                     GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("add_aco_fields")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(flag_article_collections GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("add_isbn_issn")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(add_isbns_or_issns_to_articles GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("extract_keywords_from_titles")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(enrich_keywords_with_title_words GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                 GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                                 /usr/local/var/lib/tuelib/stopwords.???  >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("flag_electronic_and_open_access")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(flag_electronic_and_open_access_records GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                         GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("augment_bible_references")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(augment_time_aspects GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                      Normdaten-"${date}".mrc \
                      GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("augment_canon_law_references")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(update_ixtheo_notations \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
    /usr/local/var/lib/tuelib/IxTheo_Notation.csv >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("augment_time_aspect_references")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(subfield_code_replacer GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                        GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                        "689A=q" >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("update_ixtheo_notations")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(map_ddc_to_ixtheo_notations \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
    /usr/local/var/lib/tuelib/ddc_ixtheo.map >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("replace_689a_689q")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(add_synonyms \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    Normdaten-partially-augmented2-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("map_ddc_to_ixtheo_notations")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(augment_773a --verbose GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("add_keyword_synonyms_from_authority")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(add_synonyms \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    Normdaten-partially-augmented2-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("fill_missing_773a")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(augment_773a --verbose GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("tag_further_potential_relbib_entries")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(add_additional_relbib_entries GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                               GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("integrate_sort_year_for_serials")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(add_publication_year_to_serials \
    Schriftenreihen-Sortierung-"${date}".txt \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("integrate_refterms")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(add_referenceterms Hinweissätze-Ergebnisse-"${date}".txt GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("tag_tue_records_with_ita_field")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(flag_records_as_available_in_tuebingen \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("add_entries_for_subscription_bundles")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(add_subscription_bundle_entries GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                 GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("add_tags_for_subsystems")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(add_subsystem_tags GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc Normdaten-"${date}".mrc \
                    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("appending_literary_remains")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(create_literary_remains_records GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                                 GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc \
                                 Normdaten-partially-augmented2-"${date}".mrc \
                                 Normdaten-fully-augmented-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("tag_pda_candidates")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
# Use the most recent GVI PPN list.
(augment_pda \
    $(ls -t gvi_ppn_list-??????.txt | head -1) \
    GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("patch_transitive_records")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(find_transitive_record_set GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
                            GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc 2>&1 \
                            dangling_references.list && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("cross_link_type_tagging")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(add_cross_link_type GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("tag_inferior_records")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(add_is_superior_work_for_subsystems GesamtTiteldaten-post-phase"$((PHASE-1))"-"${date}".mrc \
    GesamtTiteldaten-post-pipeline-"${date}".mrc >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""

phase_iter = get_phase_by_key("check_record_integrity_end")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
(marc_check --do-not-abort-on-empty-subfields --do-not-abort-on-invalid-repeated-fields \
            GesamtTiteldaten-post-pipeline-"${date}".mrc \
    >> "${log}" 2>&1 && \
EndPhase || Abort) &
"""


phase_iter = get_phase_by_key("cleanup")
phase_iter.command = \
    "StartPhase " + '"' + phase_iter.name  + '"' + \
r"""
for p in $(seq 0 "$((PHASE-2))"); do
    rm -f GesamtTiteldaten-post-phase"$p"-??????.mrc
done
rm -f child_refs child_titles parent_refs
EndPhase
"""

Main()

