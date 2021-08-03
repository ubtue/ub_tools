#!/bin/bash

# This script updates the local zotero-enhancement-maps repository with
# auto-generated maps and pushes the changes to GitHub
set -o errexit -o nounset

# Set up the log file:
readonly LOGDIR=/usr/local/var/log/tuefind
readonly LOG_FILENAME=$(basename "$0")
readonly LOG="${LOGDIR}/${LOG_FILENAME%.*}.log"

function Usage {
    echo "usage: $0 TEST|LIVE email"
    echo "       TEST = git pull, update files, no commit/push, no email, no log"
    echo "       LIVE = git pull, update files, git commit/push,"
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
    rm -f "${LOG}"
fi

readonly WORKING_DIR=/usr/local/var/lib/tuelib/zotero-enhancement-maps
readonly CONFIG_PATH=zotero_harvester.conf
readonly GITHUB_SSH_KEY=~/.ssh/github-robot
readonly FIELDS_TO_UPDATE=NAME,ONLINE_PPN,ONLINE_ISSN,PRINT_PPN,PRINT_ISSN,UPLOAD_OPERATION,LICENSE,PERSONALIZED_AUTHORS,EXPECTED_LANGUAGES,ENTRY_POINT_URL,UPDATE_WINDOW,SELECTIVE_EVALUATION

function Echo {
    if [[ "$MODE" = "LIVE" ]]; then
        echo -e "$1\n" | tee --append "${LOG}"
    elif [[ "$MODE" = "TEST" ]]; then
        echo -e "$1"
    fi
}

no_problems_found=false
ssh_agent_pid=0
function SendEmail {
    if [[ $ssh_agent_pid != 0 && $(ps -p $ssh_agent_pid) ]]; then
        Echo "cleanup: killing ssh-agent pid $ssh_agent_pid"
        kill $ssh_agent_pid
    fi

    if [ "$no_problems_found" = true ]; then
        if [[ "$MODE" = "LIVE" ]]; then
            send_email --priority=low --recipients="$EMAIL_ADDRESS" \
                       --subject="$0 passed on $(hostname)" --message-body="No problems were encountered."
        fi
        Echo "*** ZOTERO ENHANCEMENT MAPS UPDATE DONE ***"
        exit 0
    else
        if [[ "$MODE" = "LIVE" ]]; then
            send_email --priority=high --recipients="$EMAIL_ADDRESS" \
                       --subject="$0 failed on $(hostname)" \
                       --message-body="Check the log file at $LOG for details."
        fi
        Echo "*** ZOTERO ENHANCEMENT MAPS UPDATE FAILED ***"
        exit 1
    fi
}
trap SendEmail EXIT

Echo "*** ZOTERO ENHANCEMENT MAPS UPDATE START ***"
cd $WORKING_DIR

Echo "Checking for changes before start"
set +o errexit
git diff --no-patch --exit-code
config_modified=$?
set -o errexit
if [ $config_modified -ne 0 ]; then
    Echo "Uncommitted changes detected, aborting..."
    exit 1
fi

Echo "Start SSH agent"
eval "$(ssh-agent -s)"
ssh_agent_pid=$(pgrep -n ssh-agent)
ssh-add "$GITHUB_SSH_KEY"

Echo "Pull changes from upstream"
if [[ "$MODE" = "TEST" ]]; then
    git pull
elif [[ "$MODE" = "LIVE" ]]; then
    git pull >> "${LOG}" 2>&1
fi

Echo "Import changes from Zeder instance IxTheo"
if [[ "$MODE" = "LIVE" ]]; then
    zeder_to_zotero_importer        \
        --min-log-level=DEBUG       \
        $CONFIG_PATH                \
        UPDATE                      \
        IXTHEO                      \
        '*'                         \
        $FIELDS_TO_UPDATE           \
        >> "${LOG}" 2>&1
    zeder_to_zotero_importer        \
        --min-log-level=DEBUG       \
        $CONFIG_PATH                \
        IMPORT                      \
        IXTHEO                      \
        '*'                         \
        >> "${LOG}" 2>&1
elif [[ "$MODE" = "TEST" ]]; then
    zeder_to_zotero_importer        \
        --min-log-level=DEBUG       \
        $CONFIG_PATH                \
        UPDATE                      \
        IXTHEO                      \
        '*'                         \
        $FIELDS_TO_UPDATE
    zeder_to_zotero_importer        \
        --min-log-level=DEBUG       \
        $CONFIG_PATH                \
        IMPORT                      \
        IXTHEO                      \
        '*'
fi

Echo "Import changes from Zeder instance KrimDok"
if [[ "$MODE" = "LIVE" ]]; then
    zeder_to_zotero_importer        \
        --min-log-level=DEBUG       \
        $CONFIG_PATH                \
        UPDATE                      \
        KRIMDOK                     \
        '*'                         \
        $FIELDS_TO_UPDATE           \
        >> "${LOG}" 2>&1
    zeder_to_zotero_importer        \
        --min-log-level=DEBUG       \
        $CONFIG_PATH                \
        IMPORT                      \
        KRIMDOK                     \
        '*'                         \
        >> "${LOG}" 2>&1
elif [[ "$MODE" = "TEST" ]]; then
    zeder_to_zotero_importer        \
        --min-log-level=DEBUG       \
        $CONFIG_PATH                \
        UPDATE                      \
        KRIMDOK                     \
        '*'                         \
        $FIELDS_TO_UPDATE
    zeder_to_zotero_importer        \
        --min-log-level=DEBUG       \
        $CONFIG_PATH                \
        IMPORT                      \
        KRIMDOK                     \
        '*'
fi

if [[ "$MODE" = "LIVE" ]]; then
    set +o errexit
    git diff --no-patch --exit-code
    config_modified=$?
    set -o errexit
    if [ $config_modified -ne 1 ]; then
        Echo "No new changes to commit"
    else
        Echo "Push changes to GitHub"
        git add *
        git commit "--author=\"ubtue_robot <>\"" "-mUpdates via cronjob"
        git push
    fi
fi

no_problems_found=true
exit 0
