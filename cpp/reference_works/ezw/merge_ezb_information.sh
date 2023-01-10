#!/bin/bash
# Expects a ;-separated version of the original EZW-xsls and the 
set -o errexit -o nounset -o pipefail

if [ $# != 3 ]; then
    echo "Usage: $0 ezw_base_csv ezw_additional_csv csv_out"
    exit 1
fi

trap CleanUpTmpFiles EXIT

NUM_OF_TMPFILES=3
function GenerateTmpFiles {
    for i in $(seq 1 ${NUM_OF_TMPFILES}); do
        tmpfiles+=($(mktemp --tmpdir $(basename -- "${csv_additional_in%.*}").XXXX."${csv_additional_in##*.}"));
    done
}


function CleanUpTmpFiles {
   for tmpfile in ${tmpfiles[@]}; do rm ${tmpfile}; done
}

csv_in="$1"
csv_additional_in="$2"
csv_out="$3"

tmpfiles=()
GenerateTmpFiles

#Convert the original CSV file
cat "${csv_in}" | csvtool -t ';' col 1,2 - | tail -n +3  > ${tmpfiles[1]}

#Convert and fix the addtionally provided CSV file
cat ${csv_additional_in} | sed -r -e 's/""/"/g' | \
    tail -n +3 | sed -r -e 's/^"//' | sed -r -e 's/;\s*$//' | \
    `# fix erroneous field` \
    sed -r -e 's/";"/;/' | sed -r -e 's/""Die Gemeinde""/`Die Gemeinde`/' | \
    csvtool col 13,3,21 - > ${tmpfiles[2]}

# Join the the two informations
csvtool join 1 2,3  ${tmpfiles[1]} ${tmpfiles[2]} > ${csv_out}
