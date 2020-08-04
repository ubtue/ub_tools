#!/bin/bash
set -o errexit -o nounset


BACKUP_DIR=/usr/local/var/lib/tuelib/sent_newsletters
mkdir --parents "$BACKUP_DIR"
REPLY_TO=johannes.ruscheinski@uni-tuebingen.de
SQL_QUERY="SELECT 'johannes.ruscheinski@uni-tuebingen.de'"
declare -i count=0
for regular_file in $(ls -p | grep -v /); do
    bulk_mailer "$regular_file" \
                "$SQL_QUERY" \
                /usr/local/var/lib/tuelib/auxillary_newsletter_email_addresses \
                $REPLY_TO
    let "count=count+1"
    mv "$regular_file" "$BACKUP_DIR/regular_file.$(date +%F-%T)"
done
echo "Successfully mailed $count newsletter(s)"
