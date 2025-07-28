#/bin/bash

if [ $# != 1 ]; then
   echo "Usage $0 sample_records.json"
   exit 1
fi 

sample_file="$1"

cat ${sample_file} | jq -r '.[].record.id' | xargs -P 10 -I '{}' \
    /bin/bash -c ' &>2 echo "Handling $1"; \
        outfile=$PWD/new_samples_new_items/$1.txt;
        cd ../..
        source bin/activate; \
        python mixture_of_agents_text.py $1 \
        > ${outfile}' _ '{}'
