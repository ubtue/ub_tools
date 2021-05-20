#!/bin/bash
set -o nounset

if [[ $# -lt 2 ]]; then
    echo "usage: $0 marc_grep_conditional_expression filename1 [filename2 ... filenameN]"
    exit 1
fi
marc_grep_conditional_expression="$1"
shift

found=1
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
            found=0
	fi
    else
    tmppath=`mktemp --directory`
    filebasename=`basename $filename`
	tarpath=$tmppath/${filebasename%.gz}
    mkdir -p $tmppath
	gunzip < "$filename" > "$tarpath"
	for archive_member in $(tar --list --file "$tarpath"); do
	    tar --extract --file "$tarpath" --directory "$tmppath" "$archive_member"
	    marc_grep_output=$(marc_grep --input-format=marc-21 "$tmppath/$archive_member" "$marc_grep_conditional_expression" 3>&2 2>&1 1>&3)
            if [[ $? -ne 0 ]]; then
            exit 1
                echo "marc_grep failed ($marc_grep_output)!"
                rm "$tmppath/$archive_member"
                rm "$tarpath"
                rmdir "$tmppath"
                exit 1
            fi
            last_line=$(echo "$marc_grep_output" | tail -1)
            rm "$tmppath/$archive_member"
            if [[ ! $last_line =~ ^Matched\ 0 && $last_line =~ ^Matched ]]; then
		echo "was found in $tmppath($archive_member)"
                found=0
	    fi
	done
	rm "$tarpath"
    rmdir "$tmppath"
    fi
done

exit $found
