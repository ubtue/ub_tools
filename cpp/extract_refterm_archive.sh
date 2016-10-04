#!/bin/bash
set -o errexit -o nounset

usage() {
    cat << EOF
Converts a given gzipped tar archive with reference data to a plain MARC21 file
USAGE: ${0##*/} REFDATA_ARCHIVE.tar.gz OUTPUT_FILENAME
EOF
}

if [[ "$#" -ne 2 ]]; then
     echo "Invalid number of parameters"
     usage
     exit 1
fi

archive_filename="$1"
output_filename="$2"

> "$output_filename"
tar_filename="${archive_filename%.gz}"
gunzip < "$archive_filename" > "$tar_filename"
for archive_member in $(tar --list --file "$tar_filename"); do
    tar --extract --file "$tar_filename" "$archive_member"
    if [[ -s "$archive_member" ]]; then
        marc_grep "$archive_member" 'if "001"==".*" extract *' marc_binary >> "$output_filename"
    fi
done
rm "$tar_filename"
