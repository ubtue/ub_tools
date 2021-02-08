#!/bin/bash
HMAC_FILE=/usr/local/vufind/local/tuefind/local_overrides/hmac.conf
if [ ! -f ${HMAC_FILE} ]; then
    HASH=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 32 | head -n 1)
    echo "HMACkey = $HASH" > $HMAC_FILE
fi

