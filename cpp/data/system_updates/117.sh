#!/bin/bash
set -o errexit

if [[ -e /usr/local/vufind ]]; then
    apt-get --yes install node-grunt-cli
fi
