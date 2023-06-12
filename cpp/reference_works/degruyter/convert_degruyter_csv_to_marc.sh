#!/bin/bash
set -o errexit -o nounset

trap RemoveTempFiles EXIT

AUTHORS_FILE="authors.txt"

function RemoveTempFiles {
   for tmpfile in ${tmpfiles[@]}; do
       rm ${tmpfile}
   done
}

if [[ $# != 2 ]]; then
   echo "Usage: $0 degruyter_reference_work.csv out_file.xml"
   exit 1
fi

csv_file="$1"
out_file="$2"


tmpfiles=()
tmp_file1=$(mktemp -t marc_convert_$(basename ${csv_file%.csv})_XXXXX.xml)
tmp_file2=$(mktemp -t marc_convert_$(basename ${csv_file%.csv})_XXXXX.xml)
tmpfiles+=(${tmp_file1})
tmpfiles+=(${tmp_file2})
./convert_degruyter_csv_to_marc <(cat ${csv_file} | \
    sed -re 's/,+$//g' | sed 1d) ${tmp_file1}
./clean_and_convert_degruyter_refworks.sh ${tmp_file1} ${tmp_file2}
./add_author_gnds_to_marc ${tmp_file2} ${out_file} ${AUTHORS_FILE}
marc_grep ${out_file} 'if "001"==".*" extract *' traditional > ${out_file%.xml}.txt
