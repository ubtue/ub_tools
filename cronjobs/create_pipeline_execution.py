#!/bin/python3
# -*- coding: utf-8 -*-

# Algorithm to bring pipeline phases in an ideal order
# change from python 3.9 graphlib to toposort because it 
# not only provides a flatten order but also a order with alternatives

import argparse
from toposort import toposort
from itertools import product

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
        Phase("extract_normdata_translations","i_","n","Extract Normdata Translations"),
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
        Phase("check_record_integrity_end","i_","t","Check Record Integrity at the End of the Pipeline"),
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
            if phase_title_norm == "x_n":
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
    parser.add_argument("--max-group-size", default=3, type=int, help="(max.) number of elements inside a running group")
    args = parser.parse_args()

    is_debug = False
    max_group_size = args.max_group_size

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

    try:
        order_topo = list(toposort(graph))
        all_combinations = list(product(*order_topo))
        for elem in all_combinations:
            split_and_write_pipeline_steps(elem, max_group_size, False) #sets global var. fifo_counter
            if fifo_counter > best_fifo:
                best_fifo = fifo_counter
                best_order = elem
    except:
        print("Error: cross reference / cicle in graph")
        exit()

    split_and_write_pipeline_steps(best_order, max_group_size, True)
    print("Created: ", fifo_counter, " fifos")
    print("")

Main()

