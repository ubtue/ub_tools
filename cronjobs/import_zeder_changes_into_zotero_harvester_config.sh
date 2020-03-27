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
    if [ "$no_problems_found" = true ]; then
        send_email --priority=low --sender="zts_harvester_delivery_pipeline@uni-tuebingen.de" --recipients="$email_address" \
                   --subject="$0 passed on $(hostname)" --message-body="No problems were encountered."
        exit 0
    else
        send_email --priority=high --sender="zts_harvester_delivery_pipeline@uni-tuebingen.de" --recipients="$email_address" \
                   --subject="$0 failed on $(hostname)" \
                   --message-body="Check the log file at $log for details."
        echo "*** ZEDER CHANGES TO ZOTERO UPDATE FAILED ***" | tee --append "${log}"
        exit 1
    fi
}
trap SendEmail EXIT


function Usage {
    echo "usage: $0 email"
    echo "       email = email address to which notifications are sent upon (un)successful completion of the update"
    exit 1
}


function EndUpdate {
    echo -e "*** ZEDER CHANGES TO ZOTERO UPDATE DONE ***" | tee --append "${log}"
    no_problems_found=true
    exit 0
}


if [ $# != 1 ]; then
    Usage
fi


readonly email_address=$1
readonly working_dir=/usr/local/ub_tools/cpp/data
readonly config_path=zotero_harvester.conf
readonly github_ssh_key=~/.ssh/github-robot
readonly fields_to_update=UPLOAD_OPERATION


echo -e "*** ZEDER CHANGES TO ZOTERO UPDATE START ***\n" | tee --append "${log}"
cd $working_dir


echo -e "Start SSH agent\n" | tee --append "${log}"
eval "$(ssh-agent -s)"
ssh-add "$github_ssh_key"


echo -e "Pull changes from upstream\n" | tee --append "${log}"
git pull >> "${log}" 2>&1


echo -e "Import changes from Zeder instance IxTheo\n" | tee --append "${log}"
zeder_to_zotero_importer        \
    --min-log-level=DEBUG       \
    $config_path                \
    UPDATE                      \
    IXTHEO                      \
    '*'                         \
    $fields_to_update           \
    >> "${log}" 2>&1


echo -e "Import changes from Zeder instance KrimDok\n" | tee --append "${log}"
zeder_to_zotero_importer        \
    --min-log-level=DEBUG       \
    $config_path                \
    UPDATE                      \
    KRIMDOK                     \
    '*'                         \
    $fields_to_update           \
    >> "${log}" 2>&1


git diff --exit-code $config_path
config_modified=$?
if [ $config_modified -ne 0]; then
    echo -e "No new changes to commit\n" | tee --append "${log}"
    EndUpdate
fi


echo -e "Push changes to GitHub\n" | tee --append "${log}"
git add $config_path
git commit "--author=\"ubtue_robot <>\"" "-mUpdated fields from Zeder"
git push


EndUpdate
