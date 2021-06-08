#!/bin/python3
# -*- coding: utf-8 -*-

# Algorithm to bring pipeline phases in an ideal order

import argparse

class Phase:
    key = "" 
    mode = ""
    name = ""
    title_norm = ""
    command = ""
    def __init__(self, _key, _mode, _title_norm):
        self.key = _key
        self.mode = _mode
        self.title_norm = _title_norm

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
        Phase("3","","t"),
        Phase("4","","t"),
        Phase("5","","t"),
        Phase("6","","t"),
        Phase("14","","t"),
        Phase("7","","t"),
        Phase("8","","t"),
        Phase("9","_i","t"),
        Phase("13","","t"),
        Phase("a","","t"),
        Phase("b","","t"),
        Phase("c","","t"),
        Phase("k","","t"),
        Phase("m","","t"),
        Phase("x","","t"),
        Phase("z","","t")
        }

def get_phase_by_key(key):
    for phase in phases:
        if phase.key == key:
            return phase
    return None

def contains_phase_title_norm(title_norm):
    for phase in phases:
        if phase.title_norm == title_norm:
            return True
    return False

# e.g. ["5","3"] means that "5" can only be run after "3"
deps = [["5","3"],["5","14"],["7","4"],["8","5"],["9","7"],["6","13"],["3","13"],["b","a"],["c","a"],["m","k"],["z","x"],["z","k"],["c","b"]]
#deps = [["5","3"],["7","4"],["8","5"],["_9","7"],["6","13"],["3","13"],["3","8"]] #not valid

def split_to_equal_parts(lst, n):
    chunks = []
    each_chunk = []
    last_phase_title_norm = None
    for elem in lst:
        phase_mode = get_phase_by_key(elem).mode
        phase_title_norm = get_phase_by_key(elem).title_norm
        if phase_mode.startswith("_") == False and (last_phase_title_norm == None or last_phase_title_norm == phase_title_norm):
            each_chunk.append(elem) 
        else:
            if len(each_chunk) > 0:
                chunks.append(each_chunk)
                each_chunk = []
            each_chunk.append(elem)
        if phase_mode.endswith("_") == True:
            chunks.append(each_chunk)
            each_chunk = []
        if len(each_chunk) == n or elem == lst[-1]:
            if len(each_chunk) > 0:
                chunks.append(each_chunk)
            each_chunk = []
        last_phase_title_norm = phase_title_norm 
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

    # Checking if all phases used in deps are defined
    for elem in deps:
        if not any(x.key == elem[0] for x in phases):
            print(elem[0], " used in dependency: ", elem, " but not defined in phases")
            exit()
        if not any(x.key == elem[1] for x in phases):
            print(elem[1], " used in dependency: ", elem, " but not defined in phases")
            exit()
        if get_phase_by_key(elem[0]).title_norm != get_phase_by_key(elem[1]).title_norm:
            print("Dependecies of Phases must not be mixed: ", elem[0], " and ", elem[1])
            exit()

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
    while len(grouped) > number_of_groups and len(grouped) > 1:
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
        if not contains_elem(deps, elem.key):
            if len(totalorder) < number_of_groups:
                totalorder.append([elem.key])
            else:
                [min_position, min_list_ignore] = min_length_list(totalorder)
                totalorder[min_position].append(elem.key)


    if is_debug:
        print("Added phases without dependencies: ", totalorder)

    split_total_order = []
    for elem in totalorder:
        split_total_order.append(list(split_to_equal_parts(elem, items_in_group)))

    print("Split order of phases in running group: ", split_total_order)

    print("")

Main()

