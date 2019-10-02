#/bin/bash
#Helper program for extracting valid records and importing them them to mongo
set -o errexit -o nounset

if [ $# != 1 ]; then
    echo Extract valid OADOI records and import them to the oadoi mongo database
    echo Usage: $0 changed_dois_with_versions_XXXXX.jsonl.gz
    exit 1
fi

zcat $1 | sed -e 's#^\\N$##' | sed -s 's#\\\\"#\\"#'g | jq -R 'fromjson? | .' | mongoimport --db oadoi --collection all_oadoi --mode upsert --upsertFields doi
