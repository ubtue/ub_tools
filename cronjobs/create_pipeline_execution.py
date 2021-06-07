#!/bin/python3
# -*- coding: utf-8 -*-

# Algorithm to bring pipeline phases in an ideal order

import argparse

# Dependency list has to be built dynamically later
# Preprocessing
# Check Record Integrity at the Beginning of the Pipeline
# marc_check --do-not-abort-on-empty-subfields --do-not-abort-on-invalid-repeated-fields \
#            --write-data=GesamtTiteldaten-post-phase"$PHASE"-"${date}".mrc GesamtTiteldaten-"${date}".mrc >> "${log}" 2>&1

# convention in dict-keys: 
#   leading "_" can only run after predecessor has finished
#   trailing "_" successor must not run before job has finished
#   leading and trailing "_" has to run independently
phases = {}
phases["remove_dangling_references"] = ["Remove Dangling References"]
phases["add_local_data_from_database"] = ["Add Local Data from Database"]
phases["swap_and_delete_ppns"] = ["Swap and Delete PPN's in Various Databases"]
phases["filter_856_etc"] = ["Filter out Self-referential 856 Fields AND Remove Sorting Chars From Title Subfields AND Remove blmsh Subject Heading Terms AND Fix Local Keyword Capitalisations AND Standardise German B.C. Year References"]
phases["rewrite_authors_from_authority_data"] = ["Rewrite Authors and Standardized Keywords from Authority Data"]
phases["add_missing_cross_links"] = ["Add Missing Cross Links Between Records"]
phases["extract_translations"] = ["Extract Translations and Generate Interface Translation Files"]
phases["transfer_880_to_750"] = ["Transfer 880 Authority Data Translations to 750"]
phases["augment_authority_data_with_keyword_translations"] = ["Augment Authority Data with Keyword Translations"]
phases["add_beacon_to_authority_data"] = ["Add BEACON Information to Authority Data"]
phases["cross_link_articles"] = ["Cross Link Articles"]
phases["normalize_urls"] = ["Normalise URL's"]
phases["parent_to_child_linking"] = ["Parent-to-Child Linking and Flagging of Subscribable Items"]
phases["populate_zeder_journal"] = ["Populate the Zeder Journal Timeliness Database Table"]
phases["add_additional_open_access"] = ["Add Additional Open Access URL's"]
phases["extract_normdata_translations"] = ["Extract Normdata Translations"]
phases["add_author_synomyns_from_authority"] = ["Add Author Synonyms from Authority Data"]
phases["add_aco_fields"] = ["Add ACO Fields to Records That Are Article Collections"]
phases["add_isbn_issn"] = ["Adding of ISBN's and ISSN's to Component Parts"]
phases["extract_keywords_from_titles"] = ["Extracting Keywords from Titles"]
phases["flag_electronic_and_open_access"] = ["Flag Electronic and Open-Access Records"]
phases["augment_bible_references"] = ["Augment Bible References"]
phases["augment_canon_law_references"] = ["Augment Canon Law References"]
phases["augment_time_aspect_references"] = ["Augment Time Aspect References"]
phases["update_ixtheo_notations"] = ["Update IxTheo Notations"]
phases["replace_689a_689q"] = ["Replace 689\$A with 689\$q"]
phases["map_ddc_to_ixtheo_notations"] = ["Map DDC to IxTheo Notations"]
phases["add_keyword_synonyms_from_authority_data"] = ["Add Keyword Synonyms from Authority Data"]
phases["fill_missing_773a"] = ["Fill in missing 773\$a Subfields"]
phases["tag_further_potential_relbib_entries"] = ["Tag further potential relbib entries"]
phases["integrate_sort_year_for_serials"] = ["Integrate Reasonable Sort Year for Serials"]
phases["integrate_refterms"] = ["Integrate Refterms"]
phases["tag_tue_records_with_ita_field"] = ["Tag Records that are Available in Tübingen with an ITA Field"]
phases["add_entries_for_subscription_bundles"] = ["Add Entries for Subscription Bundles and Tag Journals"]
phases["add_tags_for_subsystems"] = ["Add Tags for subsystems"]
phases["appending_literary_remains"] = ["Appending Literary Remains Records"]
phases["tag_pda_candidates"] = ["Tag PDA candidates"]
phases["patch_transitive_records"] = ["Patch Transitive Church Law, Religous Studies and Bible Studies Records"]
phases["cross_link_type_tagging"] = ["Cross-link Type Tagging"]
phases["tag_inferior_records"] = ["Tags Which Subsystems have Inferior Records in Superior Works Records"]
phases["_check_end_of_pipeline"] = ["Check Record Integrity at the End of the Pipeline"]
phases["_cleanup_"] = ["Cleanup of Intermediate Files"]

#phases = {"3":"n.a","4":"n.a","5":"n.a","6":"n.a","14":"n.a","7":"n.a","8":"n.a","_9":"n.a","13":"n.a","a":"n.a","b":"n.a","c":"n.a","k":"n.a","m":"n.a","x":"n.a","z":"n.a"}

# e.g. ["5","3"] means that "5" can only be run after "3"
deps = [
        ["_cleanup_", "_check_end_of_pipeline"],
        ["_check_end_of_pipeline", "tag_inferior_records"]
       ]
#deps = [["5","3"],["5","14"],["7","4"],["8","5"],["_9","7"],["6","13"],["3","13"],["b","a"],["c","a"],["m","k"],["z","x"],["z","k"],["c","b"]]
#deps = [["5","3"],["7","4"],["8","5"],["_9","7"],["6","13"],["3","13"],["3","8"]] #not valid

def split_to_equal_parts(lst, n):
    chunks = []
    each_chunk = []
    for elem in lst:
        if elem.startswith("_") == False:
            each_chunk.append(elem) 
        else:
            if len(each_chunk) > 0:
                chunks.append(each_chunk)
                each_chunk = []
            each_chunk.append(elem)
        if elem.endswith("_") == True:
            chunks.append(each_chunk)
            each_chunk = []
        if len(each_chunk) == n or elem == lst[-1]:
            if len(each_chunk) > 0:
                chunks.append(each_chunk)
            each_chunk = []
    return chunks


def contains_elem(lst_2d, elem):
    for lst in lst_2d:
        if elem in lst:
            return True
    return False

def min_length_list(input_list):
    #min_length = min(len(x) for x in input_list )
    min_list = min(input_list, key = len)
    min_pos = input_list.index(min_list)
    return(min_pos, min_list)

def Main():
    parser = argparse.ArgumentParser(description='bring pipeline phases in an ideal order')
    parser.add_argument("--par_runs", default=2, type=int, help="(max.) number of separate running groups executed at the same time")
    parser.add_argument("--run_steps", default=3, type=int, help="(max.) number of elements inside a running group")
    args = parser.parse_args()

    is_debug = False
    # Number of parallel running groups
    number_of_groups = args.par_runs # def. 2, 1 = without parallelization
    # Number of running phases in a running group
    items_in_group = args.run_steps

    # determine independent groups
    # if both phases of a dependency have not yet occured -> create new group
    # if one phase of a dependency is already in a group -> add phases and dependency to that group
    # moving phases and dependency if phases are in different groups, e.g.
    # [3,5] -> new group, [6,13] -> new group, [3,13] -> add to [3,5] -> move [6,13] to [[3,5],[3,13]]

    if is_debug:
        print("\nDependencies: ", deps)

    moved = []
    grouped = [] 
    for elem in deps:
        found = -1 
        for i in range(len(grouped)):
            if contains_elem(grouped[i],elem[0]) or contains_elem(grouped[i],elem[1]):
                if found > -1:
                    # store for later replacement
                    moved.append([found,i])
                else:
                    grouped[i].append(elem)
                    print("adding: ",elem)
                    found = i 
        if found == -1:
            grouped.append([elem])
            print("creating: ",elem)

    deleted_elems = set() 
    for elem in moved:
        print("Moving: ", grouped[elem[1]], " to ", grouped[elem[0]])
        grouped[elem[0]].extend(grouped[elem[1]])
        deleted_elems.add(elem[1])
    # list element must be deleted from right to left to keep order
    deleted_elems = sorted(deleted_elems)
    deleted_elems.reverse()
    for elem in deleted_elems:
        del grouped[elem]

    if is_debug:
        print("Grouping possibilities dependencies only: ", grouped)

    # rearrange groups to number of parallel running groups
    while len(grouped) > number_of_groups and len(grouped)>1:
        [min_position, min_list] = min_length_list(grouped)
        del grouped[min_position]
        [min_position_new, min_list_ignore] = min_length_list(grouped) #adding to new min after deletion
        grouped[min_position_new].extend(min_list)

    if is_debug:
        print("Grouping rearranged to ", number_of_groups, " groups: ", grouped)

    totalorder = []
    for dep in grouped:
        order = []
        for elem in dep:
            if elem[1] not in order:
                order.insert(0,elem[1])
            order.insert(order.index(elem[1])+1, elem[0])
            # eliminate dupplicates using latest (rightest position) version
            order.reverse()
            order = list(dict.fromkeys(order))
            order.reverse()

        # check for cross / wrong references
        for elem in dep:
            if order.index(elem[0]) < order.index(elem[1]):
                print("Error: cross reference at " + elem[0] + "<-" + elem[1], " order: ", order)
                exit()

        totalorder.append(order)
    
    if is_debug:
        print("Order in running group: ", totalorder)

    # Adding phases without dependencies
    for elem in phases:
        if not contains_elem(deps, elem):
            if len(totalorder) < number_of_groups:
                totalorder.append([elem])
            else:
                [min_position, min_list_ignore] = min_length_list(totalorder)
                totalorder[min_position].append(elem)

    # Checking if all phases used in deps are defined
    for elem in deps:
        if elem[0] not in phases:
            print(elem[0], " used in dependency: ", elem, " but not defined in phases")
            exit()
        if elem[1] not in phases:
            print(elem[1], " used in dependency: ", elem, " but not defined in phases")
            exit()

    if is_debug:
        print("Added phases without dependencies: ", totalorder)

    split_total_order = []
    for elem in totalorder:
        split_total_order.append(list(split_to_equal_parts(elem, items_in_group)))

    print("Split order of phases in running group: ", split_total_order)

    print("")

Main()

