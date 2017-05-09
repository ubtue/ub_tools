#!/bin/bash
set -o errexit -o nounset

# Creates a list of PPNs of superior records for later processing by the add_superior_flag tool.

if [ $# -ne 1 ]; then
    echo "usage: $0 marc_input"
    exit 1
fi

rm --force superior_ppns

marc_grep $1 '"800w:810w:830w:773w:776w"' \
    | grep '(DE-576)' \
    | sed -r 's/^([^:]+)[^)]+[)](.+)$/\2/' \
    | sort \
    | uniq \
    > superior_ppns
