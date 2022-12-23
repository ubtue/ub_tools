#!/bin/bash
set -o errexit -o nounset


NEWSLETTER_DIR=/usr/local/var/lib/tuelib/newsletters
BACKUP_DIR=/usr/local/var/lib/tuelib/newsletters/sent
mkdir --parents "$BACKUP_DIR"
SENDER_AND_REPLY_TO=krimdok-team@ub.uni-tuebingen.de
declare -i count=0
for regular_file in $(ls -p "$NEWSLETTER_DIR" | grep -v /); do
    bulk_mailer "$regular_file" \
                "SELECT email FROM vufind.user WHERE krimdok_subscribed_to_newsletter = TRUE" \
                /usr/local/var/lib/tuelib/krimdok_auxiliary_newsletter_email_addresses \
                $SENDER_AND_REPLY_TO
    let "count=count+1"
    mv "$regular_file" "$BACKUP_DIR/regular_file.$(date +%F-%T)"
done
echo "Successfully mailed $count newsletter(s)"
