#!/bin/bash
# The documentation on how to access the CORE API is at https://api.core.ac.uk/docs/v3
set -o errexit -o nounset

if [ $# != 0 ]; then
    echo "Usage: $0"
    exit 1
fi


# Generate a file that will be used by convert_core_json_to_marc later in this script:
extract_zeder_data /usr/local/var/lib/tuelib/print_issns_titles_online_ppns_and_online_issns.csv krimdok tit eppn essn


declare -r WORK_FILE=download_krimdok_core.json
declare -i SINGLE_CURL_DOWNLOAD_MAX_TIME=200 # in seconds
declare -r API_KEY=$(< /usr/local/var/lib/tuelib/CORE-API.key)
declare -r CORE_API_URL=https://api.core.ac.uk/v3/search/works
declare -r -i MAX_HITS_PER_REQUEST=100 # The v3 API does not allow more than 100 hits per request!
declare -r TIMESTAMP_FILE=/usr/local/var/lib/tuelib/CORE-KrimDok.timestamp


# load the timestamp or use a hard-coded default:
if [ -r "$TIMESTAMP_FILE" ]; then
    TIMESTAMP=$(date --date="$(< "$TIMESTAMP_FILE") -1 day" +%Y-%m-%d)
else
    TIMESTAMP="2021-01-01"
fi
echo "Using Timestamp: $TIMESTAMP"


#declare -r QUERY=$(/usr/local/bin/urlencode "(title:criminology OR title:criminological OR title:kriminologie) AND createdDate>$TIMESTAMP")
declare -r QUERY=$(/usr/local/bin/urlencode "(title:criminology OR title:criminological OR title:kriminologie) AND createdDate<2019-01-01")


echo "Before curl download..."
declare -i offset=0
curl --max-time $SINGLE_CURL_DOWNLOAD_MAX_TIME --header "Authorization: Bearer ${API_KEY}" --request GET \
     --location "${CORE_API_URL}?offset=$offset&limit=$MAX_HITS_PER_REQUEST&entityType=works&q=$QUERY&scroll" \
     > $WORK_FILE
echo "After curl download..."


declare -r error_message=$(jq .error\?.message < "$WORK_FILE" 2>/dev/null)
if [[ $error_message != "" && $error_message != "null" ]]; then
    echo "Server reported: $error_message"
    exit 2
fi


declare -r total_hits=$(jq ."[0].totalHits" < "$WORK_FILE" 2>/dev/null)
if [[ $total_hits == "null" || $total_hits == "0" ]]; then
    echo "Server reported zero hits."
    exit 3
fi


if grep --quiet '504 Gateway Time-out' "$WORK_FILE"; then
    echo "We got a 504 Gateway Time-out"
    exit 4
fi


declare -r message=$(jq .message < "$WORK_FILE" 2>/dev/null)
if [[ $message != "" && $message != "null" ]]; then
    echo "Server reported: $message"
    exit 5
fi

# Convert to MARC:
echo "Before conversion to MARC..."
declare -r MARC_OUTPUT=KrimDok-CORE-$(date +%Y%M%d).xml
convert_core_json_to_marc --create-unique-id-db --sigil=DE-2619 "$WORK_FILE" unmapped_issn.list "$MARC_OUTPUT"
echo "Generated $MARC_OUTPUT, unmapped ISSN's are in unmapped_issn.list"


# Update contents of the timestamp file:
date --iso-8601=date > "$TIMESTAMP_FILE"


#upload_to_bsz_ftp_server.py "$MARC_OUTPUT" /pub/UBTuebingen_Default/
