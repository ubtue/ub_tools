#!/bin/bash
set -o errexit -o nounset

source_file=${1}
intermediate="intermediate.xml"
target_file=${source_file%.xml.gz}.mrc
split_files='out-*.xml'

time zcat ${source_file} | sed -r -e 's/[<]([/])?marc:/<\1/g' | sed -r -e 's/(xmlns:marc=["]http:[/][/]www.loc.gov[/]MARC21[/]slim["])//g'  | sed -r -e 's/xmlns=["]{2}//' | xml_split -s 1G
for i in $(ls ${split_files}); do tidy -xml -o ${i} ${i}; done
xmlstarlet ed -L -O ${split_files}
xml_merge ${split_files} > ${intermediate}
time marc_convert ${intermediate} ${target_file}
> ${intermediate}
rm ${split_files}
