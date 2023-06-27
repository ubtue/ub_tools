#!/bin/bash
set -o errexit

if [[ $TUEFIND_FLAVOUR == "krimdok" ]] || [[ $TUEFIND_FLAVOUR == "ixtheo" ]] ; then
    OLD_DIR=$PWD
    cd /usr/local/vufind
    composer install --no-interaction
    cd "$OLD_DIR"
fi
