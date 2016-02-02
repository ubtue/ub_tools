#!/bin/bash
set -o errexit -o nounset

if [[ $# != 2 ]]; then
    echo "usage: $0 email_address file_with_ids_to_delete"
    echo "       \"email_address\" is where the report about successful deletions"
    echo "       and deletion failures will be  sent."
    echo "       \"file_with_ids_to_delete\" should contain the control numbers of"
    echo "       records we wish to delete.  One number per line."
    exit 1
fi


EMAIL_ADDRESS="$1"
INPUT_FILE="$2"
DELETION_LOG="/tmp/deletion.log"
MAX_IDS_PER_CALL=1000

function ExecCurl() {
    local id_list
    id_list="$1"
    result=$(curl "http://localhost:8080/solr/biblio/update?commit=true" \
             --silent --show-error \
             --data "<delete><query>id:($id_list)</query></delete>" \
             --header 'Content-type:text/xml; charset=utf-8')
    if ! $(echo "$result" | grep -q '<int name="status">0</int>'); then
	echo "Failed to call SOLR! (id_list = $id_list)" >> "$DELETION_LOG"
    fi
}


> "$DELETION_LOG"
counter=0
id_list=""
while read line; do
    if [[ ${#line} < 13 ]]; then
	echo "Weird short line: $line"
	continue
    fi

    # Record types are documented here: https://wiki.bsz-bw.de/doku.php?id=v-team:daten:datendienste:sekkor
    # 'A' are title records and '9' are local data records.
    record_type=${line:11:1}
    if [[ "$record_type" == "A" ]]; then
        id=${line:12}
    elif [[ "$record_type" == "9" ]]; then
        id=${line:12:9}
    else
	continue
    fi

    if [[ $counter -eq $MAX_IDS_PER_CALL ]]; then
        ExecCurl "$id_list"
	counter=0
	id_list=""
    elif [[ $counter == 0 ]]; then
	id_list="$id"
	((++counter))
    else
	id_list+=" OR $id"
	((++counter))
    fi	  
done < "$INPUT_FILE"

if [[ "$id_list" != "" ]]; then
    ExecCurl "$id_list"
fi

# Only mail the log if an error occurred:
if [[ -s "$DELETION_LOG" ]]; then 
    mailx -s "Import Deletion Log (Errors)" "$1" < "$DELETION_LOG"
fi
