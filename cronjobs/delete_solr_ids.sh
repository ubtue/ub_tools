#!/bin/bash

if [[ $# != 2 ]]; then
    echo "usage: $0 email_address $0 file_with_ids_to_delete"
    echo "       \"email_address\" is where the report about successful deletions"
    echo "       and deletion failures will be  sent."
    echo "       \"file_with_ids_to_delete\" should contain the control numbers of"
    echo "       records we wish to delete.  One number per line."
    exit 1
fi

EMAIL_ADDRESS="$1"
INPUT_FILE="$2"
DELETION_LOG="/tmp/deletion.log"

> "$DELETION_LOG"
while read id; do
    result=$(curl "http://localhost:8080/solr/biblio/update?commit=true" \
             --data "<delete><query>$id</query></delete>" \
             --header 'Content-type:text/xml; charset=utf-8')
    if $(echo "$result" | grep -q '<int name="status">0</int>'); then
	echo "Successfully deleted control number $id" >> "$DELETION_LOG"
    else
	echo "Failed to delete control number $id" >> "$DELETION_LOG"
    fi
done < "$INPUT_FILE"

mailx -s "Import Deletion Log" "$1" < "$DELETION_LOG"
