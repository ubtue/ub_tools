#!/bin/bash

if [ $# != 1 ]; then
    echo "Usage: $0 afo_abbrev_file_1.pdf"
    exit 1
fi

K10PLUS_DATA_BASE_URL="https://mb064.ub.uni-tuebingen.de/api/v1/search?"
K10PLUS_DATE_TITLE_SEARCH_BASE_QUERY=${K10PLUS_DATA_BASE_URL}"type=Title&sort=relevance%2C%20year%20desc&page=1&limit=20&lookfor="
QUERY_TMP_DIR="/tmp/association"
FILE_TO_CONVERT="${1}"
tmpfile=$(mktemp)
uncolumned_tmpfile=$(mktemp)


pdftotext -layout "${FILE_TO_CONVERT}" - | grep -v "AfO Abkürzungen Liste 1 − Stand: AfO 53 (2015)" | grep -v "^Archiv für Orientforschung$" > ${tmpfile}

> ${uncolumned_tmpfile}
cut -c 1-57 ${tmpfile} >> ${uncolumned_tmpfile}
cut -c 58- ${tmpfile} >> ${uncolumned_tmpfile}
rm ${tmpfile}
cat ${uncolumned_tmpfile} | grep -v '^$' |  awk '/^[[:blank:]]{,6}/ {gsub(/^ {,6}/, "", $0); print }' |
    awk '!/^[[:blank:]]/ {gsub(/ +$/, " ", $0); gsub(/-$/, "", $0); printf "\n%s", $0}; \
         /^[[:blank:]]/ {gsub(/^ +/, " ", $0);  gsub(/ +$/, "", $0); printf "%s", $0}' |
    awk 'gsub(/[[:blank:]]+/, " ", $0)' | sponge ${uncolumned_tmpfile}
cat ${uncolumned_tmpfile} | grep ' = ' | awk -F' = ' '{print $1 "|" $2}' | sponge ${uncolumned_tmpfile}
cat ${uncolumned_tmpfile} | awk -F '|' '{gsub(/^[^,]+, /, "", $2); gsub(/[(].*[)]/, "", $2); print $1"|"$2}' | grep -v ')$' | sponge ${uncolumned_tmpfile}
cat ${uncolumned_tmpfile} | grep -E '^[A-Z]{2,6}[|]' | sponge ${uncolumned_tmpfile}

declare -A abbrevs_and_title_candidates="($(awk -F'|' '{print "[\""$1"\"]=\""$2"\""}' ${uncolumned_tmpfile}))"

mkdir ${QUERY_TMP_DIR}
for entry in ${!abbrevs_and_title_candidates[@]}; do
    # Clear away separation problems, trim, etc.
    value=$(echo "${abbrevs_and_title_candidates[${entry}]}" | perl -ne 's/(?<![\s])-\s(?!\s)//; s/^\s+|\s+$//g; print')
    echo "THE ABBREV IS \"${entry}\" AND SEARCH STRING IS \"${value}\""
    curl --insecure -X GET "${K10PLUS_DATE_TITLE_SEARCH_BASE_QUERY}"$(urlencode "${value}") -H "accept: application/json" | tee ${QUERY_TMP_DIR}/${entry}.txt
    echo "-----------------------------------------------------------------"
done



rm ${uncolumned_tmpfile}
