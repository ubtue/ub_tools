#!/bin/bash
set -o errexit -o nounset

if [[ $# < 2 ]]; then
    echo "usage: $0 marc_grep_conditional_expression filename1 [filename2 ... filenameN]"
    exit 1
fi
marc_grep_conditional_expression="$1"
shift

for filename in "$@"; do
    if [[ $filename =~ \.gz$ ]]; then
	actual_filename=${filename%.gz}
	trap 'rm -f "$actual_filename"' ERR
	gunzip < "$filename" > "$actual_filename"
    else
	actual_filename="$filename"
    fi
    marc_grep_output=$(marc_grep "$actual_filename" "$marc_grep_conditional_expression" 3>&2 2>&1 1>&3 \
                       |tail -1)
    if [[ $marc_grep_output != "Matched 0 "* ]]; then
	echo "was found in $filename"
    fi
done
