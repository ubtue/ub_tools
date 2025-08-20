#!/bin/bash
set -o errexit -o nounset -o pipefail

SUPERIOR_PRINT_RECORDS_TMP_FILE=$(mktemp --tmpdir=/tmp --suffix .xml $(basename $0)XXXX)
echo ${SUPERIOR_PRINT_RECORDS_TMP_FILE}

function CleanUp {
  rm -f ${SUPERIOR_PRINT_RECORDS_TMP_FILE}
}

trap CleanUp EXIT


if [ $# != 3 ]; then
    echo "Usage: $0 nacjd_file record_outfile map_outfile"
    exit 1
fi

nacjd_file="$1"
record_outfile="$2"
map_outfile="$3"

# Extract all print articles with superior linking and get the
# superior online PPN. Then individually download the records by SRU

marc_grep_ng --query='"007"=="tu" AND "773w"==/[(]DE-627[)].*/' --output="773w" "${nacjd_file}" | \
sed -re 's/("[(]DE-627[)])|("$)//g' | sort | uniq | \
xargs -L1 /bin/bash -c \
   'curl -s "https://sru.k10plus.de/opac-de-627?version=1.1&operation=searchRetrieve&query=pica.ppn%3D$1&maximumRecords=10&recordSchema=marcxml"; sleep .5;' _ | \
grep -v  '<?xml version="1.0" encoding="UTF-8"?>' | \
sed -re 's#[<]/?zs:.*([<]record xmlns)#\1#g' | \
sed -re 's#</zs:.*##' | \
sed -re 's#<zs:searchRetrieve.*##' > ${SUPERIOR_PRINT_RECORDS_TMP_FILE}

# Convert the results to a format that is usable by our tools
{ echo '<?xml version="1.0" encoding="UTF-8"?>'; echo '<collection xmlns="http://www.loc.gov/MARC21/slim" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd">'; cat ${SUPERIOR_PRINT_RECORDS_TMP_FILE}; echo '</collection>' ; } | sponge ${SUPERIOR_PRINT_RECORDS_TMP_FILE}

cp --archive --verbose ${SUPERIOR_PRINT_RECORDS_TMP_FILE} ${record_outfile}

xmlstarlet tr xsl/extract_print_parallel_work.xsl ${SUPERIOR_PRINT_RECORDS_TMP_FILE} > ${map_outfile}
