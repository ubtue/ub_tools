#!/bin/bash
# Datenlieferung an schweizer Kooperationspartner.
set -o errexit -o nounset

PASSWORD="EsIst1FehlerAufgetreten"

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
           --recipients=johannes.ruscheinski@uni-tuebingen.de,martin.fassnacht@uni-tuebingen.de,waldvogel@globethics.net,support@novalogix.ch \
           --subject="Neue IxTheo-Daten verfügbar" \
           --message-body="URL: http://ixtheo.de/docs/"${output}".7z\nSincerely,
Das IxTheo Team
--
Falls es irgendwelche Fragen oder Probleme mit dieser Datenlieferung gab,
bitte kontaktieren Sie
ixtheo@ub.uni-tuebingen.de  <mailto:ixtheo@ub.uni-tuebingen.de>
oder antworten Sie einfach auf diese Email.
"
