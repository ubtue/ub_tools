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

# Extract only authority records and concatenate them:
tar --gzip --extract --to-stdout --wildcards '*c???.raw' --file "$archive_filename" > "$output_filename"
