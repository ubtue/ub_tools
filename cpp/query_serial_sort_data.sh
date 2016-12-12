#!/bin/bash
# For serials derive the sort date from works that
# have this PPN as a superior work
set -o errexit -o nounset

if [ $# -ne 2 ]; then
      echo "usage: $0 marc_title_input serial_sort_list"
      exit 1;
fi

marc_title_input="$1"
serial_sort_list="$2"

tmpdir="/mnt/zram/serial_ppn_tmp"

mkdir "$tmpdir"

serial_ppns_name="serial_ppns"
serial_ppns="$tmpdir"/"$serial_ppns_name"

#Extract all serial PPNs
echo "Extract serial PPNs from \"$marc_title_input\" into \"$serial_ppns\""
create_serial_ppns.sh "$marc_title_input" "$serial_ppns"

#echo "Find the oldest inferior work for each PPN" 
> "$tmpdir"/"$serial_sort_list"
cat "$serial_ppns" | xargs  -I '{}' --max-procs=8 get_sort_date_for_serial_ppn.sh  '{}' "$tmpdir"/"$serial_sort_list"

cp "$tmpdir"/"$serial_sort_list" .






