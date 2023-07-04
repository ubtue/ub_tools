#!/bin/bash
# Extract the matched record from the authority file
set -o errexit -o nounset


function Usage {
    echo "Usage: $0 Tpi-Sätze_ixtheo.txt authority_file records_file"
    exit 1
}

MRC_STDOUT="/tmp/stdout.mrc"

function CleanUp {
  if [ -f ${MRC_STDOUT} ]; then
      rm ${MRC_STDOUT}
  fi
}

trap CleanUp EXIT

if [[ $# != 3 ]]; then
    Usage
fi

input="$1"
authority_file="$2"
records_file="$3"

read -r -d '' record_extract_script <<'EOF' || true
    authority_file="$1"
    shift
    records_file="$1"
    shift
    STDOUT="$1"
    shift
    ln -sf /dev/stdout ${STDOUT}
    marc_convert ${authority_file} ${STDOUT} ${@} | sponge -a ${records_file}
EOF

>${records_file}
cat ${input} | awk -F';' '{print $(NF-1)}' | sed -re 's/^\s*797\s*//' | \
    grep -Ev '400 ' | \
    xargs --max-args 1000  bash -c "${record_extract_script}" _  ${authority_file} ${records_file} ${MRC_STDOUT}

