#!/bin/bash

if [ $# != 2 ]; then
    echo $0 solr_work.json outfile
    exit 1
fi 

works=${1}
outfile=${2}
all_lines=$(cat "${works}" | jq -r -C '.response.docs[] | "\(.title)"' | wc -l)

>${outfile}
tmpfile=$(mktemp  "${TMPDIR:-/tmp/}$(basename $0).XXXXXXXXXXXX")
cat ${works} | jq -r -C '.response.docs[] | "\(.title)|\(.author[0])|\(.publishDate[0])"' > ${tmpfile}

for ((i = 1; i < ${all_lines}; i+=1000));
do
    cat ${tmpfile} | sed -n ${i},$((${i} + 1000))' p' | \
	 awk  '{split($0,a,"|"); cmd="control_number_guesser --lookup-title=\""a[1]"\" | head -n 1"; if (cmd | getline result) { print a[1] " | " result; } else { print a[1];} ; }' >> ${outfile} 
done
rm ${tmpfile}
