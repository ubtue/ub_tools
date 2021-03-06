#!/bin/bash
# Datenlieferung an schweizer Kooperationspartner.
set -o errexit -o nounset

readonly CONFIG_PATH=/usr/local/var/lib/tuelib/cologne.conf
readonly RECIPIENTS=$(inifile_lookup "$CONFIG_PATH" "" recipients)
readonly PASSWORD=$(inifile_lookup "$CONFIG_PATH" "" password)

cd /usr/local/ub_tools/bsz_daten
newest=$(ls -t GesamtTiteldaten-??????.mrc | head -1)
date="${newest:17:6}"
output=IxTheoDaten-"${date}".xml
marc_filter "${newest}" "${output}" --remove-fields 'LOK:.*'
rm --force /usr/local/vufind/public/docs/IxTheoDaten-*.xml.7z
7za u -p"${PASSWORD}" /usr/local/vufind/public/docs/"${output}".7z "${output}"
rm "${output}"
chcon unconfined_u:object_r:usr_t:s0 /usr/local/vufind/public/docs/"${output}".7z
send_email --sender=ixtheo@ub.uni-tuebingen.de \
           --recipients=$RECIPIENTS \
           --subject="Neue IxTheo-Daten verfügbar" \
           --message-body="URL: http://ixtheo.de/docs/"${output}".7z\\n
Das Passwort für die 7-Zip-Datei (https://de.wikipedia.org/wiki/7-Zip) lautet "$PASSWORD".\\n
Das IxTheo Team\\n
--\\n
Falls es irgendwelche Fragen oder Probleme mit dieser Datenlieferung gab,\\n
bitte kontaktieren Sie\\n
ixtheo@ub.uni-tuebingen.de  <mailto:ixtheo@ub.uni-tuebingen.de>\\n
oder antworten Sie einfach auf diese Email.\\n
"
