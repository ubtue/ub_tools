#!/bin/bash
#Setup a Kyotocabinet database and convert data
set -o errexit -o nounset

KCNAME="/usr/local/tmp/KCKONKORDANZ.db"

function Usage {
    echo "usage: $0 [--create-database bsz_concordance_file] issn_to_superior_map outfile"
}


function SetupDatabase {
   local mapping_file=$1
   kchashmgr create -otr -tc ${KCNAME}
   echo "Created database"
   local fifo_file=$(mktemp -u kcfifoXXXX)
   mkfifo "${fifo_file}"
   cat "${mapping_file}" | sed -e 's/[[:space:]]/\t/g' > "${fifo_file}" &
   kchashmgr import "${KCNAME}" ${fifo_file}
   rm ${fifo_file}
}


if [[ $# != 2 && $# != 4 ]]; then
    Usage
    exit
fi

if [ $# == 4 ]; then
   if [ $1 != "--create-database" ]; then
       Usage
       exit
   fi
   path=$2
   issn_to_superior_map=$3
   outfile=$4
   SetupDatabase $path
else 
   issn_to_superior_map=$1
   outfile=$2
fi

# Parse the file and use index approach since there can be several # in the comment section
cat ISSN_to_superior_ppn.map |  awk -F'[=#]' '{st = index($0, "#"); "kchashmgr get '${KCNAME}' " $2 | getline new_ppn;  printf "%s=%s #%s\n",$1, new_ppn, substr($0, st+1)}' > ${outfile}
