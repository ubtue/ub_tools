#!/bin/python3
# -*- coding: utf-8 -*-

# Algorithm to bring pipeline phases in an ideal order

def split_to_equal_parts(lst, n):
        for x in range(0, len(lst), n):
            each_chunk = lst[x: n+x]
            if len(each_chunk) < n:
                each_chunk = each_chunk + [None for y in range(n-len(each_chunk))]
            yield each_chunk

def contains_elem(lst_2d, elem):
    result = False
    for lst in lst_2d:
        if elem in lst:
            result = True
    return result

def min_length_list(input_list):
    #min_length = min(len(x) for x in input_list )
    min_list = min(input_list, key = len)
    min_pos = input_list.index(min_list)
    return(min_pos, min_list)

def Main():

    # Number of parallel FIFO groups
    number_of_groups = 2 # = 1 without parallelization
    # Number of FIFO phases in a FIFO group
    items_in_group = 3
    phases = ["3","4","5","6","14","7","8","9","13"]
    # Dependency list has to be built dynamically later
    # e.g. ["5","3"] means that "5" can only be run after "3"
    deps = [["5","3"],["7","4"],["8","5"],["9","7"],["6","13"],["3","13"]]
    #deps = [["5","3"],["7","4"],["8","5"],["9","7"],["6","13"],["3","13"],["3","8"]] #not valid
    print("\nDependencies: ", deps)

    # determine independent groups
    # if both phases of a dependency have not yet occured -> create new group
    # if one phase of a dependency is already in a group -> add phases and dependency to that group
    # moving phases and dependency if phases are in different groups, e.g.
    # [3,5] -> new group, [6,13] -> new group, [3,13] -> add to [3,5] -> move [6,13] to [[3,5],[3,13]]
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

    print("Grouping possibilities dependencies only: ", grouped)

    # rearrange groups to number of parallel FIFO groups
    while len(grouped) > number_of_groups and len(grouped)>1:
        [min_position, min_list] = min_length_list(grouped)
        del grouped[min_position]
        [min_position_new, min_list_ignore] = min_length_list(grouped) #adding to new min after deletion
        grouped[min_position_new].extend(min_list)

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
                print("Error: cross reference at " + elem[0] + "<-" + elem[1])
                exit()

        totalorder.append(order)
    
    print("Order in FIFO group: ", totalorder)

    # Adding phases without dependencies
    for elem in phases:
        if not contains_elem(deps, elem):
            [min_position, min_list_ignore] = min_length_list(totalorder)
            totalorder[min_position].append(elem)

    print("Added phases without dependencies: ", totalorder)

    split_total_order = []
    for elem in totalorder:
        split_total_order.append(list(split_to_equal_parts(elem, items_in_group)))
        
    print("Split order of phases in FIFO group: ", split_total_order)

    print("")

Main()

