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

mkdir "$tmpdir" && cd "$tmpdir"

serial_ppns="serial_ppns"

#Extract all serial PPNs
create_serial_ppns.sh "$marc_title_input" "$serial_ppns"

#Now find the oldest inferior work 
> "$serial_ppns"
cat "$serial_ppns" | xargs  -I '{}' --max-proces=8 get_sort_date_for_serial_ppn.sh  '{}' "$serial_sort_list"







