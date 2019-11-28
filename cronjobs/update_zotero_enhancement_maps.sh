#!/bin/bash

# This script updates the local zotero-enhancement-maps repository with
# auto-generated maps and pushes the changes to GitHub
set -o errexit -o nounset

# Set up the log file:
readonly logdir=/usr/local/var/log/tuefind
readonly log_filename=$(basename "$0")
readonly log="${logdir}/${log_filename%.*}.log"
rm -f "${log}"

no_problems_found=false
function SendEmail {
    if [[ $no_problems_found ]]; then
        send_email --priority=low --sender="zts_harvester_delivery_pipeline@uni-tuebingen.de" --recipients="$email_address" \
                   --subject="$0 passed on $(hostname)" --message-body="No problems were encountered."
        exit 0
    else
        send_email --priority=high --sender="zts_harvester_delivery_pipeline@uni-tuebingen.de" --recipients="$email_address" \
                   --subject="$0 failed on $(hostname)" \
                   --message-body="Check the log file at $log for details."
        echo "*** ZOTERO ENHANCEMENT MAPS UPDATE FAILED ***" | tee --append "${log}"
        exit 1
    fi
}
trap SendEmail EXIT

function Usage {
    echo "usage: $0 email"
    echo "       email = email address to which notifications are sent upon (un)successful completion of the update"
    exit 1
}


if [ $# != 1 ]; then
    Usage
fi

readonly email_address=$1
readonly working_dir=/usr/local/var/lib/tuelib/zotero-enhancement-maps

echo -e "*** ZOTERO ENHANCEMENT MAPS UPDATE START ***\n" | tee --append "${log}"
cd $working_dir

echo -e "Pull changes from upstream\n" | tee --append "${log}"
git pull >> "${log}" 2>&1

echo -e "Generate maps and push changes to upstream\n" | tee --append "${log}"
generate_issn_maps_from_zeder   \
    --min-log-level=DEBUG       \
    --find-duplicate-issns      \
    --push-to-github            \
    $working_dir                \
    ssg=ISSN_to_SSG.map         \
    >> "${log}" 2>&1

echo -e "*** ZOTERO ENHANCEMENT MAPS UPDATE DONE ***" | tee --append "${log}"
no_problems_found=true
exit 0
