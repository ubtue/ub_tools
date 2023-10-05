#!/bin/bash
set -o errexit -o nounset

trap RemoveTempFiles EXIT


AUTHORS_DIRECTORY="./authors"

function RemoveTempFiles {
   for tmpfile in ${tmpfiles[@]}; do
       rm ${tmpfile}
   done
}

if [[ $# != 3 ]]; then
   echo "Usage: $0 pseudo_ppn_prefix degruyter_reference_work.csv out_file.xml"
   exit 1
fi

pseudo_ppn_prefix="$1"
csv_file="$2"
out_file="$3"


tmpfiles=()
tmp_file1=$(mktemp -t marc_convert_$(basename ${csv_file%.csv})_XXXXX.xml)
tmp_file2=$(mktemp -t marc_convert_$(basename ${csv_file%.csv})_XXXXX.xml)
tmpfiles+=(${tmp_file1})
tmpfiles+=(${tmp_file2})
./convert_degruyter_csv_to_marc ${pseudo_ppn_prefix} <(cat ${csv_file} | \
    sed -re 's/,+$//g' | sed 1d) ${tmp_file1}
./clean_and_convert_degruyter_refworks.sh ${tmp_file1} ${tmp_file2}
authors_file=${AUTHORS_DIRECTORY}/authors_${pseudo_ppn_prefix}.txt
if [! -r ${authors_file} ];
    echo "Generating authors file: ${authors_file}"
    ./extract_unique_names_and_associate.sh ${tmp_file2} ${authors_file}
fi
./add_author_gnds_to_marc ${tmp_file2} ${out_file} ${authors_file}
marc_grep ${out_file} 'if "001"==".*" extract *' traditional > ${out_file%.xml}.txt
