#!/bin/bash
# Read in the output yielded by the extract program and generate a list
# with deletion ids (the last entry for each group 
# excluded and thus preserved


if [ $# != 1 ]; then
    echo "Usage: $0 deletion_candidate_file"
    exit 1
fi


deletion_candidates_file="$1"

cat ${deletion_candidates_file} | \
     paste -d " " - - - - | \
     awk '{key=$2 FS $3 FS $4; internal_ids[key]=internal_ids[key] FS $1 }END \
         {for (k in internal_ids) print k ": "  internal_ids[k]}' | \
     xargs -L1 bash -c 'echo "# $1 $2 $3 ";  \
                        arr=(${@:4}); \
                        unset arr[-1] ; \
                        echo ${arr[@]} ' _ | \
     grep -v '^#' | tr '\n' ' ' | tr ' ' '\n'

