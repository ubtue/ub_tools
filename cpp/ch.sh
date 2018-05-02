#!/bin/bash
# Datenlieferung an schweizer Kooperationspartner.
set -o errexit -o nounset

PASSWORD="EsIst1FehlerAufgetreten"

cd /usr/local/ub_tools/bsz_daten
newest=$(ls -t GesamtTiteldaten-??????.mrc | head -1)
date="${newest:17:6}"
output=IxTheoDaten-"${date}".xml
marc_filter "${newest}" "${output}" --output-format=marc-xml --remove-fields 'LOK:.*'
7za u -p"${PASSWORD}" /usr/local/vufind/public/docs/"${output}".7z "${output}"
rm "${output}"
chcon unconfined_u:object_r:usr_t:s0 /usr/local/vufind/public/docs/"${output}".7z
send_email -reply-to=ixtheo@ub.uni-tuebingen.de --recipients=johannes.ruscheinski@uni-tuebingen.de \
           --subject="Neue IxTheo-Daten verfügbar" --message-body="URL: http://ixtheo.de/docs/"${output}".7z"
send_email 
