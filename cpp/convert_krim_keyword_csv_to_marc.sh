#!/bin/bash
set -o errexit 

if [[ $# != 2 ]]; then
    echo "Usage: $0 krim_keywords.xlsx marc_out"
    exit 1
fi

tmpdir=${TMPDIR:="/tmp"}
krim_xls="$1"
marc_out="$2"
krim_csv="$(basename "${krim_xls}" .xlsx).csv"
krim_csv_full="${tmpdir}/${krim_csv}"

function CleanUp {
   if [ -w "${tmpdir}/${krim_csv}" ]; then
       rm "${tmpdir}/${krim_csv}"
   fi
}

trap CleanUp EXIT
libreoffice --headless --convert-to 'csv:Text - txt - csv (StarCalc):44,34,0' --outdir ${tmpdir} "${krim_xls}"

cat ${krim_csv_full} | sed -r -e 's/,+$//' | sponge ${krim_csv_full}
convert_krim_keyword_csv_to_marc ${krim_csv_full} ${marc_out}
