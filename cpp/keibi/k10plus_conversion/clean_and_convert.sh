#!/bin/bash
set -o errexit -o nounset

source_file=${1}
intermediate="intermediate.xml"
target_file=${source_file%.xml.gz}.mrc
split_files="out-*.xml"
zram_dir="/mnt/zram"

#Make sure zram is present
echo -n "Check ZRAM..."
test -e /mnt/zram && mountpoint --quiet /mnt/zram || { echo "ZRAM not mounted"; exit 1; }
echo "Present"

old_working_dir=$(pwd)
cp -a ${source_file} ${zram_dir}
cd ${zram_dir}
time zcat ${source_file} | sed -r -e 's/[<]([/])?marc:/<\1/g' | sed -r -e 's/(xmlns:marc=["]http:[/][/]www.loc.gov[/]MARC21[/]slim["])//g'  | sed -r -e 's/xmlns=["]{2}//' | xml_split -s 500M
set +e
for i in $(ls ${split_files}); do 
    tidy -wrap -xml --literal-attributes 1 -quiet --new-pre-tags "leader,controlfield" --output-xml 1 ${i} | \
     sed -nre '$! N; /^.*\n<\/(leader|controlfield)>/ { s/\n//; p; b }; $ { p; q }; P; D' | \
               sed -re '/<(leader|controlfield[^>]*)>/ { N;s/\n//; }' | sponge ${i}
done
set -e
xml_merge ${split_files} > ${old_working_dir}/${intermediate}
rm ${split_files}
#Only in the RAMDISK...
rm $(basename ${source_file})
cd -
time marc_convert ${intermediate} ${target_file}
> ${intermediate}
