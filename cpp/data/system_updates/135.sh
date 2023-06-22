#!/bin/bash
set -o errexit

if [[ -e /usr/local/vufind ]]; then
    OLD_DIR=$PWD
    cd /usr/local/vufind
    composer install --no-interaction
    cd "$OLD_DIR"
fi
