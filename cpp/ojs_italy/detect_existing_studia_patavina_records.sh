#!/bin/bash
set -o errexit -o nounset -o pipefail

if [ $# != 2 ]; then
    echo "Usage: $0 solr_host_and_port studia_patavina_complete.xml"
    exit 1
fi

host_and_port="$1"
marc_file="$2"
range="1959-2011"

join -j 1 -t:   \
    <(./extract_existing_studia_patavina_records.sh \
       ${host_and_port} ${range} | \
       grep -v null | cut -d'|' -f3 | sort --field-separator=: --key=1,1 | \
       awk '{print $0 ":"}') \
    <(marc_grep ${marc_file} \
      'if "001"==".*" extract "936d:936h"' control_number_and_traditional \
      | paste -d':' - -  | awk -F: '{print  $3 " " $6  ":" $1}' | \
      sort --field-separator=: --key=1,1) | \
awk -F: '{print $3}'
