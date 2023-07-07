#!/bin/bash
# Extract Internet archive identifiers for existing records with OCoLC-035-MARC field intersected with a given time range
set -o nounset -o pipefail

trap CleanUpTmpFiles EXIT

if [ $# != 3 ]; then
    echo "Usage: $0 marc_data solr_base_url output_base.txt"
    exit 1
fi

NUM_OF_TMPFILES=4
function GenerateTmpFiles {
    for i in $(seq 1 ${NUM_OF_TMPFILES}); do
        tmpfiles+=($(mktemp --tmpdir internet_archive_${i}.XXXXX.txt));
    done
}

function CleanUpTmpFiles {
   for tmpfile in ${tmpfiles[@]}; do rm ${tmpfile}; done
}

marc_data="$1"
solr_base_url="$2"
output_base="$3"
interval=1000

tmpfilee=()
GenerateTmpFiles
marc_grep ${marc_data} '"035"'  control_number_and_traditional | grep OCoLC  | sed -re 's/:035:\s+\$a\(OCoLC\)/:/' > ${tmpfiles[0]}
curl http://${solr_base_url}':8983/solr/biblio/select?fl=id&fq=publishDate%3A%5B*%20TO%201920%5D&indent=true&q.op=OR&q=*%3A*&rows=1000000&wt=csv' > ${tmpfiles[1]}
cat ${tmpfiles[1]} | tail -n +2 | sed -re 's/,$//' > ${tmpfiles[2]}
(join -t: <(sort -t: ${tmpfiles[2]})  <(sort ${tmpfiles[0]}) > ${tmpfiles[3]})

all_target_records_count=$(cat ${tmpfiles[3]} | wc --lines)


i=0
for offset in $(seq 0 ${interval} ${all_target_records_count}); do
    ((i++))
    sed_filter=$(echo $((${offset} + 1)),$((${offset} + ${interval}))p)
    outfile=$(echo ${output_base%.*}_${i}.${output_base##*.})
    cat ${tmpfiles[3]} | sed -n ${sed_filter} |  xargs -I'{}' /bin/bash -c $'echo -n "$1|"; ia search oclc-id:$(echo "$1" | sed -re \'s/.*://\'); echo; sleep 0.5' _ '{}' \
       | tee ${outfile}
done
