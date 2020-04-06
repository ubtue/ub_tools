#!/bin/bash

# This script updates the local Zotero Harvester configuration file
# with changes imported from Zeder
set -o errexit -o nounset

readonly LOGDIR=/usr/local/var/log/tuefind
readonly LOG_FILENAME=$(basename "$0")
readonly LOG="${LOGDIR}/${LOG_FILENAME%.*}.log"

function Usage {
    echo "usage: $0 TEST|LIVE email"
    echo "       TEST = git pull, zeder pull, no email"
    echo "       LIVE = git pull, zeder pull, git commit/push,"
    echo "              result mail to given address,"
    echo "              logging to $LOG"
    exit 1
}

if [[ $# != 1 && $# != 2 ]]; then
    Usage
fi

readonly MODE=$1
if [[ $MODE != 'TEST' && $MODE != 'LIVE' ]]; then
    Usage
elif [[ $MODE == 'TEST' && $# != 1 ]]; then
    Usage
elif [[ $MODE == 'LIVE' && $# != 2 ]]; then
    Usage
fi

if [[ $MODE == 'LIVE' ]]; then
    readonly EMAIL_ADDRESS=$2
fi

readonly WORKING_DIR=/usr/local/ub_tools/cpp/data
readonly CONFIG_PATH=zotero_harvester.conf
readonly GITHUB_SSH_KEY=~/.ssh/github-robot
readonly FIELDS_TO_UPDATE=UPLOAD_OPERATION
rm -f "${LOG}"

function Echo {
    if [[ "$MODE" = "LIVE" ]]; then
        echo -e "{$1}\n" | tee --append "${LOG}"
    elif [[ "$MODE" = "TEST" ]]; then
        echo -e "$1"
    fi
}

no_problems_found=false
ssh_agent_pid=0
function Exit {
    if [[ $ssh_agent_pid != 0 && $(ps -p $ssh_agent_pid) ]]; then
        Echo "cleanup: killing ssh-agent pid $ssh_agent_pid"
        kill $ssh_agent_pid
    fi

    if [ "$no_problems_found" = true ]; then
        if [[ "$MODE" = "LIVE" ]]; then
            send_email --priority=low --sender="zts_harvester_delivery_pipeline@uni-tuebingen.de" --recipients="$EMAIL_ADDRESS" \
                       --subject="$0 passed on $(hostname)" --message-body="No problems were encountered."
        fi
        Echo "*** ZEDER CHANGES TO ZOTERO UPDATE DONE ***"
        exit 0
    else
        if [[ "$MODE" = "LIVE" ]]; then
            send_email --priority=high --sender="zts_harvester_delivery_pipeline@uni-tuebingen.de" --recipients="$EMAIL_ADDRESS" \
                   --subject="$0 failed on $(hostname)" \
                   --message-body="Check the log file at $LOG for details."
        fi
        Echo "*** ZEDER CHANGES TO ZOTERO UPDATE FAILED ***"
        exit 1
    fi
}
trap Exit EXIT

Echo "*** ZEDER CHANGES TO ZOTERO UPDATE START ***"
cd $WORKING_DIR

Echo "Start SSH agent"
eval "$(ssh-agent -s)"
ssh_agent_pid=$(pgrep -n ssh-agent)
Echo "Add SSH key"
ssh-add "$GITHUB_SSH_KEY"


Echo "Pull changes from upstream"
if [[ "$MODE" = "TEST" ]];then
    git pull
elif [[ "$MODE" = "LIVE" ]];then
    git pull >> "${LOG}" 2>&1
fi


Echo "Import changes from Zeder instance IxTheo"
zeder_to_zotero_importer        \
    --min-log-level=DEBUG       \
    $CONFIG_PATH                \
    UPDATE                      \
    IXTHEO                      \
    '*'                         \
    $FIELDS_TO_UPDATE           \
    >> "${LOG}" 2>&1


Echo "Import changes from Zeder instance KrimDok"
zeder_to_zotero_importer        \
    --min-log-level=DEBUG       \
    $CONFIG_PATH                \
    UPDATE                      \
    KRIMDOK                     \
    '*'                         \
    $FIELDS_TO_UPDATE           \
    >> "${LOG}" 2>&1


if [[ "$MODE" = "LIVE" ]]; then
    git diff --exit-code $CONFIG_PATH
    config_modified=$?
    if [ $config_modified -ne 0]; then
        Echo "No new changes to commit"
        EndUpdate
    fi


    Echo "Push changes to GitHub"
    git add $CONFIG_PATH
    git commit "--author=\"ubtue_robot <>\"" "-mUpdated fields from Zeder"
    git push
fi


no_problems_found=true
exit 0
