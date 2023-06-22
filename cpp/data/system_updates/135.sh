#!/bin/bash
set -o errexit

if [[ -e /usr/local/vufind ]]; then
    OLD_DIR=$PWD
    cd /usr/local/vufind
    sudo composer install
    cd "$OLD_DIR"
fi
