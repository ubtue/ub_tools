#!/bin/bash
set -o errexit -o nounset

# Creates a list of PPNs of superior records for later processing by the add_superior_flag tool.

if [ $# -ne 1 ]; then
    echo "usage: $0 marc_input"
    exit 1
fi

rm --force cross_links superior_ppns_candidates superior_ppns

marc_grep $1 '"776i"' \
    | grep 'Erscheint auch als' \
    | cut -d: -f1 \
    | sort \
    | uniq \
    > cross_links
           
marc_grep $1 '"800w:810w:830w:773w:776w"' \
    | grep '(DE-576)' \
    | sed -r 's/^([^:]+)[^)]+[)](.+)$/\2/' \
    | sort \
    | uniq \
    > superior_ppns_candidates

comm -23 superior_ppns_candidates cross_links > superior_ppns
