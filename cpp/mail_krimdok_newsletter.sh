#!/bin/bash
set -o errexit -o nounset


cd .


most_recent_email_contents=$(ls -Art | tail -n 1)
if [ -z "$most_recent_email_contents" ]; then
    exit 0
fi

time_since_last_modification=$(($(date +%s) - $(stat --format=%Y -- "$most_recent_email_contents")))
if [ $time_since_last_modification -gt 86400 ]; then
    exit 0
fi

bulk_mailer "$most_recent_email_contents" \
            "SELECT 'johannes.ruscheinski@uni-tuebingen.de'" \
            /mnt/ZE020150/FID-Entwicklung/KrimDok/auxillary_newsletter_email_addresses \
            johannes.ruscheinski@uni-tuebingen.de
