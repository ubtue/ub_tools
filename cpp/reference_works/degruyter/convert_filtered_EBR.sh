#/bin/bash
set -o errexit -o nounset -o pipefail

if [ $# != 3 ]; then
    echo "usage: $0 Gesamtiteldaten.mrc publisher.xslx output_dir"
    exit 1
fi

function RemoveTempFiles {
   for tmpfile in ${tmpfiles[@]}; do
       echo "Try to remove ${tmpfile}"
       rm ${tmpfile}
   done
   rmdir ${tmp_dir}
}

trap RemoveTempFiles EXIT
tmpfiles=()

marc_in="$1"
excel_in="$2"
output_dir="$3"

tmp_dir_template=$(basename $0 .sh)"_XXXXXX"
tmp_dir=$(mktemp --directory -t ${tmp_dir_template})
filtered_csv="${tmp_dir}/ebr_filtered_$(date +'%y%m%d').csv"
#tmpfiles+=(${filtered_csv})
generate_filtered_output="${tmp_dir}/generate_filtered_output.log"
tmpfiles+=(${generate_filtered_output})

./generate_filtered_EBR_csv.sh ${marc_in} ${excel_in} ${filtered_csv} 2>&1 | tee ${generate_filtered_output}

REFWORKS=$(basename ${filtered_csv} | sed -re 's/([^_]+)_.*/\1/g')
for refwork in ${REFWORKS}; do
    if [ $refwork != "ebr" ]; then
        echo "Invalid refwork ${refwork}"
        echo "Filename must match ebr_xxx.csv"
        exit 1
    fi
    echo "Converting ${refwork}"
    refwork_csv_file=$(ls ${tmp_dir}/${refwork}*.csv)
    echo "Using file ${refwork_csv_file}"
    converted_file=ixtheo_${refwork}_$(date +'%y%m%d')_001.xml
    ./convert_degruyter_csv_to_marc.sh \
        $(echo ${refwork} | awk '{print toupper($0)}')  \
        "${refwork_csv_file}" \
        "${output_dir}/${converted_file}"
    tmpfiles+=(${refwork_csv_file})
done
