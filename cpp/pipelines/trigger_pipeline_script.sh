#!/bin/bash
# Triggers Pipeline script depending on passed trigger file and event type
set -o errexit -o nounset

if [[ $# != 2 ]]; then
    echo "Usage: $0 mutex_file event"
    exit 1
fi

mutex_file="$1"
event="$2"

FETCH_MARC_UPDATES_FINISHED="/usr/local/var/tmp/bsz_download_happened" # Creation
MERGE_DIFFERENTIAL_AND_FULL_UPDATES_FINISHED="/usr/local/var/tmp/bsz_download_happened" #Deletion
CREATE_REFTERM_FINISHED="/usr/local/var/tmp/create_refterm_successful"

if [[ ${event} == "IN_CREATE" ]]; then
    case ${mutex_file} in 
        ${FETCH_MARC_UPDATES_FINISHED})
            /usr/local/bin/execute_cronjob_in_two_minutes.sh merge_differential_and_full_marc_updates
            ;;
        ${CREATE_REFTERM_FINISHED})
            /usr/local/bin/execute_cronjob_in_two_minutes.sh initiate_marc_pipeline.py
            ;;
    esac;
elif [[ ${event} == "IN_DELETE" ]]; then
    case ${mutex_file} in
        ${MERGE_DIFFERENTIAL_AND_FULL_UPDATES_FINISHED})
            /usr/local/bin/execute_cronjob_in_two_minutes.sh create_refterm_file.py
            /usr/local/bin/execute_cronjob_in_two_minutes.sh fetch_interlibraryloan_ppns.py
            ;;
    esac
fi
