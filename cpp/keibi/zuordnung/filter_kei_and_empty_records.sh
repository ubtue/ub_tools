#!/bin/bash

if [ $# != 1 ]; then
    echo "Usage: $0 keippn_to_ppn_mapping.txt"
    exit 1
fi

KEIPPN_TO_PPN_MAPPING=${1}


cat "${KEIPPN_TO_PPN_MAPPING}" |  perl -ne 's/KEI[0-9]{8}(?!:)//g; print' | perl -ne 's/:[ ]+/: /; print' | grep -P -v '^KEI[0-9]{8}:[ \t]*$'

