#!/bin/bash
# Extract records in our authority data that refer to given PND-beacon information
set -o errexit -o nounset -o pipefail
declare BEACON_URL="http://www.historische-kommission-muenchen-editionen.de/beacond/zdn.php?beacon"
declare TMPDIR="/tmp/"
declare -i STEPSIZE=1500

function DownloadBeaconFile() {
    beacon_file="$1"
    wget ${BEACON_URL} -O ${beacon_file}
}


function ExtractMatchingAuthorityRecords() {
    declare authority_file=$1
    declare outfile=$2
    # '<' needed to suppress filename in wc output
    for ((lower=1; lower < $(wc -l < ${beacon_file}); lower+=${STEPSIZE}))
    do
         upper=$((( ${lower}+${STEPSIZE})))
         # Strip beacon header, get only a chunk of stepsize records, join to a regular expression
         # and prefix with the old PND prefix ISIL
         echo "Processing entries ${lower} to ${upper}"
         match_command=$(cat ${beacon_file} | grep -v '^#'  | sed --quiet "${lower},${upper}p" | \
                         sed -e ':a;N;$!ba;s/\n/\|/g' | sed -E 's/([^|]+)/\\\\(DE-588a\\\\)\1/g')
         # >> does not seem to work thus the tee approach
         marc_grep ${authority_file} 'if "035z"=="'${match_command}'" extract "001"' marc_binary | tee --append ${outfile} 1>/dev/null
    done
}


if [ $# != 2 ]; then
    echo "usage: $0 Normdaten.mrc marc_output.mrc"
    exit 1
fi

authority_file="$1"
outfile="$2"
beacon_file=$(mktemp --tmpdir=${TMPDIR} bundesarchiv.XXXXX)
DownloadBeaconFile ${beacon_file}
>${outfile}
ExtractMatchingAuthorityRecords ${authority_file} ${outfile} ${beacon_file}
rm ${beacon_file}
