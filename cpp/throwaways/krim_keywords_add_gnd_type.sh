#!/bin/bash
#Tool for adding lobid type information to if GND number exists
#We expect the keyword in column 3 and GND code in column 6

if [ $# != 1 ]; then
    echo "Usage: $0 krimfile.csv (Keywords in Column 3 and GND code in column 6)"
    exit 1
fi



krim_keyword_file="${1}"

cat ${krim_keyword_file} | \
    csvtool -t ';' format '%(3);%(6)\n' - | \
    tr '\n' '\0' | \
    xargs -0 -I'{}' bash -c $'echo $@ | awk -F\';\' \'{ if ($2 != "") {curl_cmd="curl --fail -s  https://lobid.org/gnd/"$2".json"; \
        curl_cmd | getline curl_reply; jq_cmd="./jq_wrapper.sh \'\\\'\'" curl_reply  "\'\\\'\'"; \
        jq_cmd | getline jq_reply;  print $1 ";https://d-nb.info/gnd/"$2 ";" jq_reply; close(curl_cmd); close(jq_reply)}}\'' _ '{}'
