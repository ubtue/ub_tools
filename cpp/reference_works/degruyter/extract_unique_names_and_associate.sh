#!/bin/bash
set -o errexit -o nounset -o pipefail


if [ $# != 2 ]; then
    echo "Usage: $0 marc_file unique_authors.txt"
    exit 1
fi


marc_file="$1"
authors_file="$2"


marc_grep ${marc_file} 'if "100a" exists extract "100a"' traditional \
    | sed --regexp-extended -e 's/^100 //' > ${authors_file} 

marc_grep ${marc_file} 'if "700a" exists extract "700a"' traditional \
    | sed --regexp-extended -e 's/^700 //' >> ${authors_file}


cat ${authors_file} | sort | uniq | sponge ${authors_file}


export AUTHOR_LOOKUP_PATH="/usr/local/ub_tools/cpp/reference_works/bibelwissenschaft"
cat ${authors_file} |  tr '\n' '\0' | \
    xargs -0 -I'{}'  /bin/bash -c 'echo -e "$@":\\t$(${AUTHOR_LOOKUP_PATH}/swb_author_lookup --sloppy-filter "$@")' _ '{}' | sponge ${authors_file}
