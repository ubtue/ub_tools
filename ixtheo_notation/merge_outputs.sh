#!/bin/bash
# Merge files with new keyword 
set -o errexit -o nounset -o pipefail

if [ $# != 2 ]; then
    echo "Usage: $0 dir1 dir2"
    exit 1
fi

dir1="$1"
dir2="$2"

for file in $(find ${dir1} -type f -maxdepth 1); do
    cat ${file} ${dir2}/$(basename ${file})
    echo
    echo "---------------------------------------------------------"
done
