#!/bin/bash
set -o errexit -o nounset

if [ $# != 1 ]; then
    echo "Usage: $0 output_file"
    exit 1
fi


readonly OUTPUT_FILE=$1
> "$OUTPUT_FILE"


declare -r API_KEY=$(< /usr/local/var/lib/tuelib/CORE-API.key)
declare -r CORE_API_URL=https://core.ac.uk/api-v2/articles/search
declare -r -i CHUNK_COUNT=11
declare -r -i MAX_HITS_PER_PAGE=1000
declare -r QUERY=criminolog*
declare -i page_no=0
> chunk
while true; do
    ((++page_no))
    curl --silent --header "Content-Type: application/json" --header "apiKey:${API_KEY}" --request POST \
         --data-binary "[{ \"page\":$page_no, \"pageSize\":$MAX_HITS_PER_PAGE, \"query\":\"$QUERY\" }]" \
         $CORE_API_URL > chunk
    if [[ $(jq '.[].status' < chunk) == '"Not found"' ]]; then break; fi
    cat chunk >> $OUTPUT_FILE
    sleep 2
done
rm --force chunk


jq . < $OUTPUT_FILE > $OUTPUT_FILE.$$
mv $OUTPUT_FILE.$$ $OUTPUT_FILE

# The following sed expression merges the metadata arrays from the individually downloaded chunks into one huge
# JSON array.  It also eliminates the "status" and "totalHits" entries at the boundaries between two arrays.
sed --in-place --regexp-extended --expression \
    ':a;N;$!ba;s/\n\s+\]\n\s+}\n\]\n\[\n\s+\{\n\s+\"status\"\:\s*\"OK\",\n\s+\"totalHits\":\s+[0-9]+\s*,\n\s+\"data\"\:\s*\[\n/,\n/g' \
    $OUTPUT_FILE
