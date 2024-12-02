#!/bin/bash
# Extract entries from the publisher provided file of the 
# "Encyclopedia of the bible and its reception" that are not yet present 
# in our data stock
set -o errexit -o nounset -o pipefail


function RemoveTempFiles {
   for tmpfile in ${tmpfiles[@]}; do
       echo "Try to remove ${tmpfile}" > 2
       rm ${tmpfile}
   done
   rmdir ${tmp_dir}
}

trap RemoveTempFiles EXIT  
tmpfiles=()
tmp_dir_template="$(basename $0 .sh)_XXXXXX"
tmp_dir=$(mktemp --directory -t ${tmp_dir_template})


function ExtractMARCEBRLinks {
   echo "Extracting existing EBR links from MARC" >&2
   local marc_in="$1"
   marc_ebr_urls_file=${tmp_dir}/marc_ebr_urls.txt
   tmpfiles+=(${marc_ebr_urls_file})
   marc_grep ${marc_in} 'if "773w"=="[(]DE-627[)]'${EBRPPN}'" extract "856u"' control_number_and_traditional > ${marc_ebr_urls_file}
   declare -g marc_links=$(cat ${marc_ebr_urls_file} | awk -F':856u:' '{print $2}')
   declare -g marc_ebr_records_num=$(cat ${marc_ebr_urls_file} | awk -F: '{print $1}' | sort | uniq | wc -l)
}

function ExtractExcelEBRLinks {
    echo "Extracting Links from CSV" >&2
    local excel_in="$1"
    local 
    libreoffice --headless \
       --convert-to csv:"Text - txt - csv (StarCalc)":44,34,UTF8,1,,0,false,true,false,false,false,-1 \
       --outdir ${tmp_dir} ${excel_in}
    declare -g csv_full=${tmp_dir}/$(basename --multiple ${tmp_dir}/*.csv)
    tmpfiles+=(${csv_full})
    declare -g csv_links=$(csvtool format '%(3)\n' ${csv_full} | sed '1d' | sed -z '$ s/\n$//')
}

if [ $# != 3 ]; then
    echo "Usage: $0 Gesamtiteldaten.mrc publisher.xslx filtered.csv"
    exit 1
fi

marc_in="$1"
excel_in="$2"
out_csv="$3"
EBRPPN="612654680"

ExtractMARCEBRLinks ${marc_in}
ExtractExcelEBRLinks ${excel_in}

new_links=$(comm -13 <(printf '%s\n' ${marc_links} | grep /EBR/ | sort | uniq) \
                     <(printf '%s\n' ${csv_links} | sort | uniq))
echo "Generating filtered file" >&2
cat ${csv_full} | awk -F',' -f <(echo '$3 ~ /^url$|'$(printf '%s\n' ${new_links} | head -c -1 | tr '\n' '|' | sed -re 's#/#[/]#g')'/ {print}') > ${out_csv}
echo "Counted ${marc_ebr_records_num} EBR records" >&2
