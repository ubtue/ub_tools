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

wget https://labs.ddb.de/app/beagen/item/person/archive/latest -O archivportal-d.beacon.temp
if [ $? == 0 ]; then
    if IsResultSuspicouslyShort archivportal-d.beacon.temp; then
        error_message+=$'Obtained an empty or suspicously short file from Archivportal-d.\n'
    else
        mv archivportal-d.beacon.temp archivportal-d.beacon
    fi
else
    error_message+=$'Failed to download the Beacon file from Archivportal-D.\n'
fi

wget https://kalliope-verbund.info/beacon/beacon.txt -O kalliope.staatsbibliothek-berlin.beacon.temp
if [ $? == 0 ]; then
    if IsResultSuspicouslyShort kalliope.staatsbibliothek-berlin.beacon.temp; then
        error_message .= $'"Obtained an empty or suspicously short file from Kalliope.\n'
    else
        mv kalliope.staatsbibliothek-berlin.beacon.temp kalliope.staatsbibliothek-berlin.beacon
        sed -i -e 's/#FORMAT: GND-BEACON/#FORMAT: BEACON/g' kalliope.staatsbibliothek-berlin.beacon
    fi
else
    error_message+=$'Failed to download the Beacon file from Kalliope.\n'
fi

# additional beacon files for external references like wikipedia, e.g. ADB/NDB
declare -a beacon_files=("https://www.historische-kommission-muenchen-editionen.de/beacon_db_register.txt")
for beacon_file in "${beacon_files[@]}"
do
    wget --no-use-server-timestamps $beacon_file -O `basename $beacon_file`.temp
    if [ $? == 0 ]; then
        if IsResultSuspicouslyShort `basename $beacon_file`.temp; then
            error_message+=$'Obtained an empty or suspicously short file for `basename $beacon_file`.\n'
        else
            mv `basename $beacon_file`.temp `basename $beacon_file`
            sed -i -e '1{/^$/d}' `basename $beacon_file` #replace first line if it is a blank line (e.g. ADB/NDB)
        fi
    else
       error_message+=$'Failed to download the Beacon file for `basename $(beacon_file)`.\n'
    fi
done


if [[ ! -z "$error_message" ]]; then
    send_email --recipients="ixtheo-team@ub.uni-tuebingen.de" --subject="Beacon Download Error $(hostname)" \
               --message-body="$error_message"
fi
