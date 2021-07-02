#!/bin/bash
set -o errexit

FILE=/usr/local/vufind/import/lib/TuelibSolrmarcMixin.jar
if [ -f "$FILE" ]; then
    rm -f "$FILE"
fi
