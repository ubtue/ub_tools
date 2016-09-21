#!/bin/bash

# This script attempts to download a new TAD ACL.  If successful, it then
# updates the TAD access rights of all users.

if [[ $# != 1 ]]; then
    echo "usage: $0 notification_email_address"
    exit 1
fi

DOWNLOAD_URL="http://biber3.ub.uni-tuebingen.de/tad/acl.yaml"
DOWNLOAD_DOCUMENT="/tmp/new_tad_email_acl.yaml"
TARGET_DOCUMENT="/var/lib/tuelib/tad_email_acl.yaml"

if ! wget "$DOWNLOAD_URL" --output-document="$DOWNLOAD_DOCUMENT" 2> /dev/null; then
    mutt -H - <<END_OF_EMAIL
To: $1
Subject: $0 Failed
X-Priority: 1

Failed to download a new TAD email ACL from $DOWNLOAD_URL.
END_OF_EMAIL
elif ! diff --brief "$DOWNLOAD_DOCUMENT" "$TARGET_DOCUMENT" > /dev/null; then
    mv "$DOWNLOAD_DOCUMENT" "$TARGET_DOCUMENT"
    /usr/local/bin/set_tad_access_flag --update-all-users
fi
