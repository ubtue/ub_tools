#/bin/bash
set -o errexit -o nounset -o pipefail


if [ $# != 2 ]; then
    echo "usage: $0 degruyter_reference_works_dir output_dir"
    exit 1
fi

function RemoveTempFiles {
   for tmpfile in ${tmpfiles[@]}; do
       rm ${tmpfile}
   done
   rmdir ${tmp_dir}
}


trap RemoveTempFiles EXIT
tmpfiles=()

function ConvertExcelToCSV {
  local input_dir="$1"
  local output_dir="$2"
  libreoffice --headless \
  --convert-to csv:"Text - txt - csv (StarCalc)":44,34,UTF8,1,,0,false,true,false,false,false,-1 \
  --outdir ${output_dir} ${input_dir}/*.xlsx
}


input_dir="$1"
output_dir="$2"
tmp_dir_template="degruyter_refworks_XXXXXX"
tmp_dir=$(mktemp --directory -t ${tmp_dir_template})

ConvertExcelToCSV ${input_dir} ${tmp_dir}

REFWORKS=$(basename --multiple ${tmp_dir}/*.csv  | sed -re 's/([^_]+)_.*/\1/g')
for refwork in ${REFWORKS}; do
    echo "Converting ${refwork}"
    refwork_csv_file=$(ls ${tmp_dir}/${refwork}*.csv)
    echo "Using file ${refwork_csv_file}"
    # Strip abbreviations that are too long
    if [ ${refwork} == "BIBHERM" ]; then
        refwork=BHM
    fi
    converted_file=ixtheo_${refwork}_$(date +'%y%m%d').xml
    ./convert_degruyter_csv_to_marc.sh \
        $(echo ${refwork} | awk '{print toupper($0)}')  \
        ${refwork_csv_file} \
        "${output_dir}/${converted_file}"
    tmpfiles+=(${refwork_csv_file})
done
