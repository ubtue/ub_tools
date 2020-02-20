#!/bin/bash
set -o nounset

function IsResultEmpty() {
    item_count=$(grep '^[^#]' "$1" | wc -l)
    [ $item_count == "0" ]
}

cd /usr/local/ub_tools/bsz_daten

error_message=""

wget https://labs.ddb.de/app/beagen/item/person/archive/latest -O archivportal-d.beacon.temp
if [ $? == 0 ]; then
    if [ ! $(IsResultEmpty archivportal-d.beacon.temp) ]; then
        mv archivportal-d.beacon.temp archivportal-d.beacon
    else
        error_message .= "Obtained empty file from Kalliope"
    fi
else
    error_message .= "Failed to download the Beacon file from Archiveportal-D."
fi

wget http://kalliope.staatsbibliothek-berlin.de/beacon/beacon.txt -O kalliope.staatsbibliothek-berlin.beacon.temp
if [ $? == 0 ]; then
    if [ ! $(IsResultEmpty kalliope.staatsbibliothek-berlin.beacon.temp) ]; then
        mv kalliope.staatsbibliothek-berlin.beacon.temp kalliope.staatsbibliothek-berlin.beacon
        sed -i -e 's/#FORMAT: GND-BEACON/#FORMAT: BEACON/g' kalliope.staatsbibliothek-berlin.beacon
    else
        error_message .= "Obtained empty file from Kalliope"
    fi
else
    error_message .= "Failed to download the Beacon file from Kalliope."
fi
    
if [[ ! -z "$error_message" ]]; then
    send_email --recipients="ixtheo-team@ub.uni-tuebingen.de" --subject="Beacon Download Error $(hostname)" \
               --message-body="$error_message"
fi
