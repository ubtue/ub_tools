#!/bin/bash
# Find entries with dois from the given title data file and try to extract additional fulltext urls 
# based on the local snapshot of unpaywall data
set -o errexit -o nounset

DOIFILE="dois.txt"
TMPDIR=$(mktemp -d -p . oadoi.XXXXXXXXXX)
CHUNKLINES=10000
OUTPUTFILE="oadoi_urls.json"
DATABASE="oadoi"


function cleanup {
  rm -rf "$TMPDIR"
}

trap cleanup EXIT


function getDOIsFromTitleData() {
   # Extract plausible data
   marc_grep $1 'if "0242"=="doi" extract "024a"' no_label | grep '^10\.' > $DOIFILE
}


function createDOIToURLMap() {
    split --lines "$CHUNKLINES" --additional-suffix=".part" "$DOIFILE"
    counter=0
    > "$OUTPUTFILE"
    for part in *.part; do
        ((++counter))
        cat $part | sed  's/^/"/; s/$/"/' | sed -re ':a;N;$!ba;s/\n/, /g' \
            | sed '1i db.all_oadoi.find( { $and: [ { "doi" : { $in:  [' -  \
            | sed -e '$a] } },  { "best_oa_location" : { $type : "object" } } ] }, \
                    { "doi": 1, "best_oa_location.url" : 1, "_id": 0 }).forEach(printjson);' \
            > mongo_query"$counter".js
        mongo --quiet "$DATABASE" mongo_query"$counter".js > output"$counter".json
    done;
    # Combine and make array
    cat *.json | jq --slurp '.' > "$OUTPUTFILE"
}

# Main Program
if [ $# != 1 ]; then
    echo "usage: $0 GesamtTiteldaten-YYMMDD.mrc"
    exit 1
fi  

title_data=$1
cd "$TMPDIR"
getDOIsFromTitleData $title_data
createDOIToURLMap
mv "$OUTPUTFILE" ..
cd ..
echo "Successfully created file \"${OUTPUTFILE}\""
