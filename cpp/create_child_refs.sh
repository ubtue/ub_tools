#!/bin/bash
set -o errexit -o nounset

# Creates an inverted index from parent IDs to child IDs.  The original records have "uplinks" to parent IDs in
# fields 800w 810w 830w.  These uplinks are extracted and processed to create a file, named "child_refs",
# containing lines that look like "parentID:childID_1:childID_2:...:childID_N for later processing by the
# add_child_refs tool.  A second pass generates a map from child IDs to child titles which is stored in
# "child_titles".  This too will be used by the add_child_refs tool.

if [ $# -ne 2 ]; then
    echo "usage: $0 TitelUndLokaldaten-normalised.mrc ÜbergeordneteTitelUndLokaldaten-filtered-and-normalised.mrc"
    exit 1
fi

rm -f parent_refs child_titles
for f in "$1" "$2"; do
    for s in 800w 810w 830w 773w; do
	marc_grep2 $f '"'$s'"' | sed -r 's/^([^:]+)[^)]+[)](.+)$/\2:\1/' >> parent_refs
    done
    marc_grep2 $f '"245a"' >> child_titles
done
sort parent_refs \
| uniq \
| awk -F ":" 's != $1 || NR ==1{s=$1;if(p){print p};p=$0;next}{sub($1,"",$0);p=p""$0;}END{print p}' \
> child_refs
