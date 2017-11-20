#!/bin/bash

ENVIRONMENT_FILE=/etc/profile.d/vufind.sh
source $ENVIRONMENT_FILE


LOG_FILE=/usr/local/var/log/tuefind/set_tad_access_flag.log

if [[ $# != 1 ]]; then
    echo "$(date): script called with invalid argument count!" >> "$LOG_FILE"
    exit 1
fi

COMMAND_PATH=/usr/local/bin/set_tad_access_flag

if [ ! -x "$COMMAND_PATH" ]; then
    echo "$(date): command $COMMAND_PATH is not installed!" >> "$LOG_FILE"
    exit 1
fi

"$COMMAND_PATH" "$1" >> "$LOG_FILE" 2>&1
if [ $? != 0 ]; then
    echo "$(date): command failed!" >> "$LOG_FILE"
    exit 1
fi

