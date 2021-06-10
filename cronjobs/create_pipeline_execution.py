#!/bin/python3.9
# -*- coding: utf-8 -*-

# Algorithm to bring pipeline phases in an ideal order

import argparse
from graphlib import TopologicalSorter

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

# Dependency list has to be built dynamically later
# Preprocessing
# Check Record Integrity at the Beginning of the Pipeline
# marc_check --do-not-abort-on-empty-subfields --do-not-abort-on-invalid-repeated-fields \
#            --write-data=GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc GesamtTiteldaten-"${date}".mrc >> "${log}" 2>&1

# convention in dict-keys: 
#   leading "_" can only run after predecessor has finished
#   trailing "_" successor must not run before job has finished
#   leading and trailing "_" has to run independently

phases = {
        Phase("check_record_integrity_beginning","_i_","x","Check Record Integrity at the Beginning of the Pipeline"),
        Phase("remove_dangling_references","","t","Remove Dangling References"),
        Phase("add_local_data_from_database","","t","Add Local Data from Database"),
        Phase("swap_and_delete_ppns","i_","x","Swap and Delete PPN's in Various Databases"),
        Phase("filter_856_etc","","t","Filter out Self-referential 856 Fields AND Remove Sorting Chars From Title Subfields AND Remove blmsh Subject Heading Terms AND Fix Local Keyword Capitalisations AND Standardise German B.C. Year References"),
        Phase("rewrite_authors_from_authority_data","","t","Rewrite Authors and Standardized Keywords from Authority Data"),
        Phase("add_missing_cross_links","","t","Add Missing Cross Links Between Records"),
        Phase("extract_translations","i_","t","Extract Translations and Generate Interface Translation Files"),
        Phase("transfer_880_to_750","","n","Transfer 880 Authority Data Translations to 750"),
        Phase("augment_authority_data_with_keyword_translations","","n","Augment Authority Data with Keyword Translations"),
        Phase("add_beacon_to_authority_data","","n","Add BEACON Information to Authority Data"),
        Phase("cross_link_articles","","t","Cross Link Articles"),
        Phase("normalize_urls","","t","Normalise URL's"),
        Phase("parent_to_child_linking","","t","Parent-to-Child Linking and Flagging of Subscribable Items"),
        Phase("populate_zeder_journal","","t","Populate the Zeder Journal Timeliness Database Table"),
        Phase("add_additional_open_access","","t","Add Additional Open Access URL's"),
        Phase("extract_normdata_translations","","n","Extract Normdata Translations"),
        Phase("add_author_synomyns_from_authority","","t","Add Author Synonyms from Authority Data"),
        Phase("add_aco_fields","","t","Add ACO Fields to Records That Are Article Collections"),
        Phase("add_isbn_issn","","t","Adding of ISBN's and ISSN's to Component Parts"),
        Phase("extract_keywords_from_titles","","t","Extracting Keywords from Titles"),
        Phase("flag_electronic_and_open_access","","t","Flag Electronic and Open-Access Records"),
        Phase("augment_bible_references","","t","Augment Bible References"),
        Phase("augment_canon_law_references","","t","Augment Canon Law References"),
        Phase("augment_time_aspect_references","","t","Augment Time Aspect References"),
        Phase("update_ixtheo_notations","","t","Update IxTheo Notations"),
        Phase("replace_689a_689q","","t","Replace 689\$A with 689\$q"),
        Phase("map_ddc_to_ixtheo_notations","","t","Map DDC to IxTheo Notations"),
        Phase("fill_missing_773a","","t","Fill in missing 773\$a Subfields"),
        Phase("tag_further_potential_relbib_entries","","t","Tag further potential relbib entries"),
        Phase("integrate_sort_year_for_serials","","t","Integrate Reasonable Sort Year for Serials"),
        Phase("integrate_refterms","","t","Integrate Refterms"),
        Phase("tag_tue_records_with_ita_field","","t","Tag Records that are Available in Tübingen with an ITA Field"),
        Phase("add_entries_for_subscription_bundles","","t","Add Entries for Subscription Bundles and Tag Journals"),
        Phase("add_tags_for_subsystems","","t","Add Tags for subsystems"),
        Phase("tag_pda_candidates","","t","Tag PDA candidates"),
        Phase("patch_transitive_records","","t","Patch Transitive Church Law, Religous Studies and Bible Studies Records"),
        Phase("cross_link_type_tagging","","t","Cross-link Type Tagging"),
        Phase("tag_inferior_records","","t","Tags Which Subsystems have Inferior Records in Superior Works Records"),
        Phase("appending_literary_remains","_i_","x","Appending Literary Remains Records"),
        Phase("DUMMY","_i_","t","Dummy phase for testing"),
        Phase("add_keyword_synonyms_from_authority","_i_","x","Add Keyword Synonyms from Authority Data"),
        Phase("check_record_integrity_end","_i_","x","Check Record Integrity at the End of the Pipeline"),
        Phase("cleanup","_i_","x","Cleanup of Intermediate Files")
        }

graph = {
        "remove_dangling_references" : {"check_record_integrity_beginning"},
        "add_local_data_from_database" : {"remove_dangling_references"},
        "swap_and_delete_ppns" : {"add_local_data_from_database"},
        "filter_856_etc" : {"add_local_data_from_database"},
        "rewrite_authors_from_authority_data" : {"filter_856_etc"},
        "add_missing_cross_links" : {"rewrite_authors_from_authority_data"},
        "extract_translations" : {"check_record_integrity_beginning"},
        "transfer_880_to_750" : {"check_record_integrity_beginning"},
        "augment_authority_data_with_keyword_translations" : {"transfer_880_to_750"},
        "add_beacon_to_authority_data" : {"augment_authority_data_with_keyword_translations"},
        "cross_link_articles" : {"add_missing_cross_links"},
        "normalize_urls" : {"cross_link_articles"},
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
        "check_record_integrity_end" : {"extract_translations", "tag_inferior_records"},
        "cleanup" : {"check_record_integrity_end", "extract_translations"}
       }

def get_phase_by_key(key):
    for phase in phases:
        if phase.key == key:
            return phase
    return None

def process_chunk(lst):
    if len(lst) > 0:
        for i, elem in enumerate(lst):
            if len(lst) > 1 and i != len(lst) - 1:
                print("making fifo _ ", elem.title_norm)
            print("processing ", elem.key, " -> ", elem.title_norm)
        print("---")

def split_to_equal_parts(lst, n):
    open_fifo_title = []
    open_fifo_norm =[] 

    for elemkey in lst:
        elem = get_phase_by_key(elemkey)
        phase_mode = elem.mode
        phase_title_norm = elem.title_norm

        if phase_title_norm.startswith("x"):
            process_chunk(open_fifo_title)
            process_chunk(open_fifo_norm)
            open_fifo_title = []
            open_fifo_norm =[] 
        elif phase_mode.startswith("_"):
            if phase_title_norm.startswith("t"):
                process_chunk(open_fifo_title)
                open_fifo_title = []
            if phase_title_norm.startswith("n"):
                process_chunk(open_fifo_norm)
                open_fifo_norm = []

        if len(open_fifo_title) ==n:
            process_chunk(open_fifo_title)
            open_fifo_title = []
        if len(open_fifo_norm) ==n:
            process_chunk(open_fifo_norm)
            open_fifo_title = []

        if phase_title_norm.startswith("t"):
            open_fifo_title.append(elem)
        elif phase_title_norm.startswith("n"):
            open_fifo_norm.append(elem)
        elif phase_title_norm.startswith("x"):
            process_chunk([elem])

        if phase_mode.endswith("_"):
            if phase_title_norm.startswith("t"):
                process_chunk(open_fifo_title)
                open_fifo_title = []
            elif phase_title_norm.startswith("n"):
                process_chunk(open_fifo_norm)
                open_fifo_title = []


def Main():
    parser = argparse.ArgumentParser(description='bring pipeline phases in an ideal order')
    parser.add_argument("--run_steps", default=3, type=int, help="(max.) number of elements inside a running group")
    args = parser.parse_args()

    is_debug = False
    # Number of running phases 
    items_in_group = args.run_steps

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
        
    # Adding phases without dependencies
    for elem in phases:
        if not elem.key in graph:
            graph[elem.key] =  {}

    order = []

    try:
        ts = TopologicalSorter(graph)
        order = list(tuple(ts.static_order()))
    except:
        print("Error: cross reference / cicle in graph")
        exit()

    print("Split order of phases in running group:")
    split_to_equal_parts(order, items_in_group)

    print("")

Main()

