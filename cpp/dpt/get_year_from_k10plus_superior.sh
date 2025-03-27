#!/bin/bash

if [ $# != 1 ]; then
    echo "Usage $0: dpt_marc_file"
    exit 1
fi

dpt_marc_file="$1"

for ppn in $(marc_grep ${dpt_marc_file}  '"773w"' | awk -F ':' '{print $3}' | sed -re 's/^[(]DE-627[)]//' \
             | sed -re 's/\s+$//' | sort | uniq ); do
    echo $ppn:$(curl --silent "https://sru.k10plus.de/opac-de-627?version=1.1&operation=searchRetrieve&query=pica.ppn%3D"$ppn"&maximumRecords=1&recordSchema=dc" | \
        xmlstarlet sel -N dc="http://purl.org/dc/elements/1.1/" -t -v "//dc:date"  | uniq)
    sleep .5

done



