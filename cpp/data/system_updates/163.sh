#!/bin/bash
set -o errexit

PERMISSIONS_API_CONF="/usr/local/vufind/local/tuefind/instances/krimdok/config/vufind/local_overrrides/permissions_api.conf"

if [ -d $(dirname "${PERMISSIONS_API_CONF}") ]; then
    touch ${PERMISSIONS_API_CONF}
fi

