#!/bin/bash
# Find entries with dois from the given title data file and try to extract additional fulltext urls 
# based on the local snapshot of unpaywall data
set -o errexit -o nounset

DOI_FILE="dois.txt"
TMP_DIR=$(mktemp -d -p . oadoi.XXXXXXXXXX)
CHUNK_LINES=10000
DATABASE="oadoi"


function CleanUp {
  rm -rf "$TMP_DIR"
}

trap CleanUp EXIT


function GetDOIsFromTitleData() {
   # Extract plausible data
   marc_grep $1 'if "0242"=="doi" extract "024a"' no_label | grep '^10\.' > $TMP_DIR/$DOI_FILE
}


function CreateDoiToUrlMap() {
    split --lines "$CHUNK_LINES" --additional-suffix=".part" "$DOI_FILE"
    counter=0
    > "$output_file"
    for part in *.part; do
        ((++counter))
        cat $part | sed  's/^/"/; s/$/"/' | sed -re ':a;N;$!ba;s/\n/, /g' \
            | sed '1i db.all_oadoi.find( { $and: [ { "doi" : { $in:  [' -  \
            | sed -e '$a] } },  { "best_oa_location" : { $type : "object" } } ] }, \
                    { "doi": 1, "best_oa_location" : 1, "_id": 0 }).forEach(printjson);' \
            > mongo_query"$counter".js
        mongo --quiet "$DATABASE" mongo_query"$counter".js > output"$counter".json
    done;
    # Combine and make array
    cat *.json | jq --slurp '.' > "$output_file"
}

# Main Program
if [ $# != 2 ]; then
    echo "usage: $0 GesamtTiteldaten-YYMMDD.mrc" output.json
    exit 1
fi  

title_data=$1
output_file=$2
GetDOIsFromTitleData $title_data
cd "$TMP_DIR"
CreateDoiToUrlMap
mv "$output_file" ..
cd ..
echo "Successfully created file \"${output_file}\""
