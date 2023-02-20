#!/bin/bash
set -o errexit -o nounset

trap RemoveTempFiles EXIT

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
tmpfiles+=(${tmp_file1})
./convert_degruyter_csv_to_marc <(cat ${csv_file} | \
    sed -re 's/,+$//g' | sed 1d) ${tmp_file1}
./clean_and_convert_degruyter_refworks.sh ${tmp_file1} ${out_file}
