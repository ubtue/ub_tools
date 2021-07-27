#!/bin/bash
set -o errexit


readonly OAI_DUPS=/usr/local/var/lib/tuelib/oai_dups.db
if [ -e $OAI_DUPS ]; then
    cp $OAI_DUPS /usr/local/var/lib/tuelib/krim_ssoar-dups.db
fi
