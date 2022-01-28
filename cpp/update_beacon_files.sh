#!/bin/bash
set -o nounset

function IsResultSuspicouslyShort() {
    declare -r -i MIN_LINE_COUNT=10
    item_count=$(grep '^[^#]' "$1" | wc --lines)
    [ $item_count -le $MIN_LINE_COUNT ]
}

cd /usr/local/ub_tools/bsz_daten
if [[ ! -e beacon_downloads ]]; then
    mkdir beacon_downloads
fi
cd beacon_downloads

error_message=""

# We frequently get a HTTP 500 error on our systems, so wait a random time up to 50 second to avoid simultaneous access
# c.f. https://blog.buberel.org/2010/07/howto-random-sleep-duration-in-bash.html
sleep $[ ( $RANDOM % 50 ) + 1 ]s

# additional beacon files for external references like wikipedia, e.g. ADB/NDB
declare -A beacon_files
# all target names containing an .lr. will be treated as literary remains in later processing steps
beacon_files["archivportal-d.lr.beacon"]="https://labs.ddb.de/app/beagen/item/person/archive/latest";
beacon_files["kalliope.staatsbibliothek-berlin.lr.beacon"]="https://kalliope-verbund.info/beacon/beacon.txt";
beacon_files["adb-ndb.beacon"]="https://www.historische-kommission-muenchen-editionen.de/beacon_db_register.txt";
for beacon_file_key in "${!beacon_files[@]}"
do
    echo "processing beacon file: $beacon_file_key from ${beacon_files[$beacon_file_key]}"
    beacon_file_key_temp="${beacon_file_key}.temp"
    wget --no-use-server-timestamps ${beacon_files[$beacon_file_key]} -O $beacon_file_key_temp
    if [ $? == 0 ]; then
        if IsResultSuspicouslyShort $beacon_file_key_temp; then
            error_message+=$'Obtained an empty or suspicously short file for $beacon_file_key`.\n'
        else
            mv $beacon_file_key_temp $beacon_file_key
            sed -i -e 's/#FORMAT: GND-BEACON/#FORMAT: BEACON/g' $beacon_file_key #kalliope.staatsbibliothek-berlin.beacon
            sed -i -e '1{/^$/d}' $beacon_file_key #replace first line if it is a blank line (e.g. ADB/NDB)
        fi
    else
       error_message+=$'Failed to download the Beacon file for $beacon_file_key.\n'
    fi
done


if [[ ! -z "$error_message" ]]; then
    send_email --recipients="ixtheo-team@ub.uni-tuebingen.de" --subject="Beacon Download Error $(hostname)" \
               --message-body="$error_message"
fi
