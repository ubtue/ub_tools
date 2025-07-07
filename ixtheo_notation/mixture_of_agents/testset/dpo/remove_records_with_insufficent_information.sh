#/bin/bash

if [ $# != 1 ]; then
    echo "Usage $0 sample_records.json"
    exit 1
fi

sample_records="$1"

cat ${sample_records}  | \
   jq 'map(
  select(
    (.record.topic_facet // null) != null
    or
    (.record.summary // null) != null
  )
)'
