#!/bin/bash
set -o errexit

FILE=/usr/local/vufind/import/lib/TuelibSolrmarcMixin.jar
if [ -f "$FILE" ]; then
    rm -f "$FILE"
fi

DIR=/usr/local/ub_tools/java/solrmarc_mixin
if [ -d "$DIR" ]; then
    rm -rf "$DIR"
fi
