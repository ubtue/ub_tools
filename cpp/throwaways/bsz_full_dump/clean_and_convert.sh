#!/bin/bash
set -o errexit -o nounset

source_file=${1}
intermediate="/mnt/zram/intermediate.xml"
target_file=${source_file%.xml.gz}.mrc

#Make sure zram is present
echo -n "Check ZRAM..."
test -e /mnt/zram && mountpoint --quiet /mnt/zram || { echo "ZRAM not mounted"; exit 1; }
echo "Present"

time zcat ${source_file} | sed -r -e 's/[<]([/])?marc:/<\1/g' | sed -r -e 's/(xmlns:marc=["]http:[/][/]www.loc.gov[/]MARC21[/]slim["])//g'  | sed -r -e 's/xmlns=["]{2}//' > ${intermediate}
time marc_convert ${intermediate} ${target_file}
> ${intermediate}
