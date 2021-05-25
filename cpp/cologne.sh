#!/bin/bash
# Datenlieferung an kölner Kooperationspartner.
set -o errexit -o nounset

readonly CONFIG_PATH=/usr/local/var/lib/tuelib/cologne.conf
readonly EMAIL=$(inifile_lookup "$CONFIG_PATH" "" email)
readonly PASSWORD=$(inifile_lookup "$CONFIG_PATH" "" password)

cd /usr/local/ub_tools/bsz_daten
readonly newest=$(ls -t GesamtTiteldaten-??????.mrc | head -1)
readonly date="${newest:17:6}"
readonly output=IxTheoDatenFürKöln-"${date}".xml
generate_zeder_subset ixtheo koe Köln "${newest}" "${output}"
rm --force /usr/local/vufind/public/docs/IxTheoDatenFürKöln-*.xml.7z
7za u -p"${PASSWORD}" /usr/local/vufind/public/docs/"${output}".7z "${output}"
rm "${output}"
chcon unconfined_u:object_r:usr_t:s0 /usr/local/vufind/public/docs/"${output}".7z
send_email --sender=ixtheo@ub.uni-tuebingen.de \
           --recipients=martin.fassnacht@uni-tuebingen.de,"$EMAIL" \
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
