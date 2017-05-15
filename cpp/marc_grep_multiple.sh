#!/bin/bash
set -o nounset

if [[ $# -lt 2 ]]; then
    echo "usage: $0 marc_grep_conditional_expression filename1 [filename2 ... filenameN]"
    exit 1
fi
marc_grep_conditional_expression="$1"
shift

for filename in "$@"; do
    if [[ ! $filename =~ \.tar\.gz$ ]]; then
	marc_grep_output=$(marc_grep "$filename" "$marc_grep_conditional_expression" 3>&2 2>&1 1>&3)
        if [[ $? -ne 0 ]]; then
            echo "marc_grep failed ($marc_grep_output)!"
            exit 1
        fi
        last_line=$(echo "$marc_grep_output" | tail -1)
        if [[ ! $last_line =~ ^Matched\ 0 && $last_line =~ ^Matched ]]; then
	    echo "was found in $filename"
	fi
    else
	tar_filename=${filename%.gz}
	gunzip < "$filename" > "$tar_filename"
	for archive_member in $(tar --list --file "$tar_filename"); do
	    tar --extract --file "$tar_filename" "$archive_member"
	    marc_grep_output=$(marc_grep "$archive_member" "$marc_grep_conditional_expression" 3>&2 2>&1 1>&3)
            if [[ $? -ne 0 ]]; then
                echo "marc_grep failed ($marc_grep_output)!"
                exit 1
            fi
            last_line=$(echo "$marc_grep_output" | tail -1)
            rm "$archive_member"
            if [[ ! $last_line =~ ^Matched\ 0 && $last_line =~ ^Matched ]]; then
		echo "was found in $tar_filename($archive_member)"
	    fi
	done
	rm "$tar_filename"
    fi
done
