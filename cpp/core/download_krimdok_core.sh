#!/bin/bash
set -o errexit -o nounset

if [ $# != 0 ]; then
    echo "Usage: $0"
    exit 1
fi


readonly WORK_FILE=download_krimdok_core.json
> "$WORK_FILE"


declare -r API_KEY=$(< /usr/local/var/lib/tuelib/CORE-API.key)
declare -r CORE_API_URL=https://core.ac.uk/api-v2/articles/search
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
    cat chunk >> "$WORK_FILE"
    sleep 2
done
rm --force chunk


jq . < "$WORK_FILE" > "$WORK_FILE".$$
mv "$WORK_FILE".$$ "$WORK_FILE"

# The following sed expression merges the metadata arrays from the individually downloaded chunks into one huge
# JSON array.  It also eliminates the "status" and "totalHits" entries at the boundaries between two arrays.
sed --in-place --regexp-extended --expression \
    ':a;N;$!ba;s/\n\s+\]\n\s+}\n\]\n\[\n\s+\{\n\s+\"status\"\:\s*\"OK\",\n\s+\"totalHits\":\s+[0-9]+\s*,\n\s+\"data\"\:\s*\[\n/,\n/g' \
    "$WORK_FILE"

# Convert to MARC:
declare -r MARC_OUTPUT=KrimDok-CORE-$(date +%Y%M%d).mrc
convert_json_to_marc --create-unique-id-db /usr/local/var/lib/tuelib/core.conf \
                     "$WORK_FILE" unmapped_issn.list "$MARC_OUTPUT"
echo "Generated $MARC_OUTPUT, unmapped ISSN's are in unmapped_issn.list"
