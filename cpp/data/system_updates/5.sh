#!/bin/bash
HMAC_FILE=/usr/local/vufind/local/tuefind/local_overrides/hmac.conf
if [ ! -z "$VUFIND_HOME" ] && [ ! -f ${HMAC_FILE} ]; then
    HASH=$(cat /dev/urandom | tr --delete --complement 'a-zA-Z0-9' | fold --width=32 | head --lines=1)
    echo "HMACkey = $HASH" > $HMAC_FILE
fi
