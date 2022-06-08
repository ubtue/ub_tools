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
core search "(criminolog* AND createdDate>=$start AND createdDate<$end)" "$DOWNLOAD_DIR"

# merge files
core merge "$DOWNLOAD_DIR" "$ARCHIVE_FILE_JSON"

# Convert to MARC:
if [ -s "$ARCHIVE_FILE_JSON" ]; then
    core convert --create-unique-id-db --935-entry=TIT:mkri --935-entry=LOK:core --sigil=DE-2619 "$ARCHIVE_FILE_JSON" "$ARCHIVE_FILE_MARC"
fi

# upload to BSZ
if [ -s "$ARCHIVE_FILE_MARC" ]; then
    #upload_to_bsz_ftp_server.py "$MARC_OUTPUT" /pub/UBTuebingen_Default/

    # Update contents of the timestamp file:
    #date --iso-8601=date > "$TIMESTAMP_FILE"
fi
