#!/bin/bash

# This script updates the local zotero-enhancement-maps repository with
# auto-generated maps and pushes the changes to GitHub
set -o errexit -o nounset

# Set up the log file:
readonly LOGDIR=/usr/local/var/log/tuefind
readonly LOG_FILENAME=$(basename "$0")
readonly LOG="${LOGDIR}/${LOG_FILENAME%.*}.log"
rm -f "${LOG}"

no_problems_found=false
ssh_agent_pid=0
function SendEmail {
    if [[ $ssh_agent_pid != 0 && $(ps -p $ssh_agent_pid) ]]; then
        Echo "cleanup: killing ssh-agent pid $ssh_agent_pid"
        kill $ssh_agent_pid
    fi

    if [ "$no_problems_found" = true ]; then
        send_email --priority=low --sender="zts_harvester_delivery_pipeline@uni-tuebingen.de" --recipients="$EMAIL_ADDRESS" \
                   --subject="$0 passed on $(hostname)" --message-body="No problems were encountered."
        exit 0
    else
        send_email --priority=high --sender="zts_harvester_delivery_pipeline@uni-tuebingen.de" --recipients="$EMAIL_ADDRESS" \
                   --subject="$0 failed on $(hostname)" \
                   --message-body="Check the log file at $LOG for details."
        echo "*** ZOTERO ENHANCEMENT MAPS UPDATE FAILED ***" | tee --append "${LOG}"
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

readonly EMAIL_ADDRESS=$1
readonly WORKING_DIR=/usr/local/var/lib/tuelib/zotero-enhancement-maps
readonly GITHUB_SSH_KEY=~/.ssh/github-robot

echo -e "*** ZOTERO ENHANCEMENT MAPS UPDATE START ***\n" | tee --append "${LOG}"
cd $WORKING_DIR

echo -e "Start SSH agent\n" | tee --append "${LOG}"
eval "$(ssh-agent -s)"
ssh_agent_pid=$(pgrep -n ssh-agent)
ssh-add "$GITHUB_SSH_KEY"

echo -e "Pull changes from upstream\n" | tee --append "${LOG}"
git pull >> "${LOG}" 2>&1

echo -e "Generate maps and push changes to upstream\n" | tee --append "${LOG}"
generate_issn_maps_from_zeder   \
    --min-log-level=DEBUG       \
    --find-duplicate-issns      \
    --push-to-github            \
    $WORKING_DIR                \
    ssg=ISSN_to_SSG.map         \
    >> "${LOG}" 2>&1

echo -e "*** ZOTERO ENHANCEMENT MAPS UPDATE DONE ***" | tee --append "${LOG}"
no_problems_found=true
exit 0
