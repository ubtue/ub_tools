#!/bin/bash

if [ $# != 2 ]; then
   echo "Usage: $0 ppns_files output_dir"
   exit 1
fi


ppns_files="$1"
output_dir="$2"

cat ${ppns_files} | xargs -I '{}' /bin/bash -c './ixtheo_notation_kimi.sh ptah nu "$1" | tee  "$2"/$1.txt ' _ '{}' ${output_dir}
