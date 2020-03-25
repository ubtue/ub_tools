#!/bin/bash
set -e

function CreatePartialFile {
    local output_file=$1
    local offset=$(echo ${output_file} | sed -r -e 's/[^0-9]*([0-9]*)([^0-9]*)$/\1/g')
    local pages_per_bulk=20
    touch ${output_file}
    echo "[" > ${output_file}
    for i in $(seq $((${pages_per_bulk} * ${offset} + 1)) $(((${pages_per_bulk} * ${offset}) + ${pages_per_bulk}))) ; do
            cat >> ${output_file} << EOF
             { "page": $i, 
               "pageSize": 100, 
               "query": "criminolog*"
             },
EOF
    done
    sed -i '$s/,//g' ${output_file}
    echo "]" >> ${output_file}
}


if [ $# != 1 ]; then
    echo "Usage: $0 query_output_file"
    exit 1
fi

output_file=$1

for i in {0..700}; do
    partial_file=${output_file/%.json/_${i}.json}
    CreatePartialFile ${partial_file}
done



