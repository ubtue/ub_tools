#!/bin/bash
set -o errexit

if [[ -e /usr/local/vufind ]]; then
    # We explicitly need to use sudo here, even if we're already root, or it will fail,
    # see https://stackoverflow.com/questions/16151018/how-to-fix-npm-throwing-error-without-sudo
    OLD_DIR=$PWD
    cd /usr/local/vufind
    sudo composer install
    cd "$OLD_DIR"
fi
