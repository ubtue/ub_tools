#!/bin/bash
# The documentation on how to access the CORE API is at https://api.core.ac.uk/docs/v3
set -o errexit -o nounset

if [ $# != 0 ]; then
    echo "Usage: $0"
    exit 1
fi

# parameters
declare -r DATETIME=$(date +%Y%m%d_%H%M%S)
declare -r DOWNLOAD_DIR=/tmp/CORE/${DATETIME}
declare -r ARCHIVE_DIR=/usr/local/var/lib/tuelib/CORE
declare -r ARCHIVE_FILE_JSON_UNFILTERED=${ARCHIVE_DIR}/${DATETIME}_unfiltered.json
declare -r ARCHIVE_FILE_JSON_FILTERED_TUEBINGEN=${ARCHIVE_DIR}/${DATETIME}_filtered_tuebingen.json
declare -r ARCHIVE_FILE_JSON_FILTERED_INCOMPLETE=${ARCHIVE_DIR}/${DATETIME}_filtered_incomplete.json
declare -r ARCHIVE_FILE_JSON_FILTERED_DUPLICATE=${ARCHIVE_DIR}/${DATETIME}_filtered_duplicate.json
declare -r ARCHIVE_FILE_JSON=${ARCHIVE_DIR}/${DATETIME}.json
declare -r ARCHIVE_FILE_MARC=${ARCHIVE_DIR}/${DATETIME}.xml
declare -r TIMESTAMP_FILE=/usr/local/var/lib/tuelib/CORE-KrimDok.timestamp
if [ -r "$TIMESTAMP_FILE" ]; then
    TIMESTAMP=$(date --date="$(< "$TIMESTAMP_FILE") -1 day" +%Y-%m-%d)
else
    TIMESTAMP="2022-01-01"
fi
echo "Using Timestamp: $TIMESTAMP"
start=$TIMESTAMP
end=$(date +%F)

# download data
echo "Downloading data"
core search "(criminolog* AND createdDate>=$start AND createdDate<$end)" "$DOWNLOAD_DIR"

# merge files
echo "Merging files"
core merge "$DOWNLOAD_DIR" "$ARCHIVE_FILE_JSON_UNFILTERED"

# filter unwanted records
echo "Filtering unwanted records"
core filter "$ARCHIVE_FILE_JSON_UNFILTERED" "$ARCHIVE_FILE_JSON" "$ARCHIVE_FILE_JSON_FILTERED_TUEBINGEN" "$ARCHIVE_FILE_JSON_FILTERED_INCOMPLETE" "$ARCHIVE_FILE_JSON_FILTERED_DUPLICATE"

# Convert to MARC & deliver:
RESULT_COUNT=$(core count "$ARCHIVE_FILE_JSON")
if [ "$RESULT_COUNT" -gt "0" ]; then
    echo "Converting to MARC"
    core convert --create-unique-id-db --935-entry=TIT:mkri --935-entry=LOK:core --sigil=DE-2619 "$ARCHIVE_FILE_JSON" "$ARCHIVE_FILE_MARC"

    # upload to BSZ
    # TODO: Generate BSZ compatible filename
    #echo "Uploading to BSZ"
    #upload_to_bsz_ftp_server.py "$ARCHIVE_FILE_MARC" /pub/UBTuebingen_Default/

    # Update contents of the timestamp file:
    #date --iso-8601=date > "$TIMESTAMP_FILE"
fi
