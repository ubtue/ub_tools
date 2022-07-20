#!/bin/bash
set -o errexit

if [[ -e /usr/local/vufind ]]; then
    apt-get --yes install npm
fi
