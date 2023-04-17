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
declare -r ARCHIVE_FILE_JSON_FILTERED=${ARCHIVE_DIR}/${DATETIME}_filtered.json
declare -r ARCHIVE_FILE_JSON=${ARCHIVE_DIR}/${DATETIME}.json
declare -r ARCHIVE_FILE_NOSUPERIOR_MARC=${ARCHIVE_DIR}/${DATETIME}.xml
declare -r ARCHIVE_FILE_MARC=${ARCHIVE_DIR}/${DATETIME}.xml
declare -r ISSN_FILE_TXT=${ARCHIVE_DIR}/${DATETIME}.txt
declare -r ISSN_FILE_MARC=${ARCHIVE_DIR}/${DATETIME}.mrc
declare -r LOG_FILE=${ARCHIVE_FILE_MARC}.log
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
core filter "$ARCHIVE_FILE_JSON_UNFILTERED" "$ARCHIVE_FILE_JSON" "$ARCHIVE_FILE_JSON_FILTERED"

# Note: this is just the basic filter (invalid languages, empty titles, etc.).
# After it, we should do additional filters based on data providers.
# "core statistics --extended " can be used to generate a list of the data provider ids and their amout of records.
# Due to data quality problems, it was agreed that we only import data from providers with at least 200 records.
# A list can be generated manually & used as input for core filter (optional last 2 parameters at end).
# Moreover, the following data providers should always be skipped (can also be achieved using the last 2 parameters with "skip" and a list containing these ids):
# - 4786 (Crossref)
# - 1926 (Publikationsserver der Universität Tübingen)
# - 725 (Heidelberger Dokumentenserver)
# - 1030 (SSOAR)
# - 923 (DIALNET)
#
# Examples (make sure to skip first, because "keep" will throw away all data provider ids that are not on the list):
# core filter "$ARCHIVE_FILE_JSON_UNFILTERED" "$ARCHIVE_FILE_JSON" "$ARCHIVE_FILE_JSON_FILTERED" skip data_providers_to_skip.csv
# core filter "$ARCHIVE_FILE_JSON_UNFILTERED" "$ARCHIVE_FILE_JSON" "$ARCHIVE_FILE_JSON_FILTERED" keep data_providers_to_keep.csv


# Convert to MARC & deliver:
RESULT_COUNT=$(core count "$ARCHIVE_FILE_JSON")
if [ "$RESULT_COUNT" -gt "0" ]; then
    echo "Converting to MARC"
    # Note: The LOG_FILE should be sent to the librarians so they know which CORE IDs could be problematic
    #       and should be checked manually after import (e.g. datasets with more than 20 authors.)
    core convert --create-unique-id-db --935-entry=TIT:mkri --935-entry=LOK:core --sigil=DE-2619 "$ARCHIVE_FILE_JSON" "$ARCHIVE_FILE_NOSUPERIOR_MARC" "$LOG_FILE"

    # ISSN lookup against library compound
    marc_grep "$ARCHIVE_FILE_NOSUPERIOR_MARC" '"773x"' no_label | sort | uniq > "$ISSN_FILE_TXT"
    issn_lookup.py "$ISSN_FILE_TXT" "$ISSN_FILE_MARC"
    marc_issn_lookup "$ARCHIVE_FILE_NOSUPERIOR_MARC" "$ISSN_FILE_MARC" "$ARCHIVE_FILE_MARC"

    # upload to BSZ
    # TODO: Generate BSZ compatible filename
    # Also: Please note that CORE data can be huge. The BSZ wants us to deliver at most 5.000 datasets per day
    #       and split the data over multiple deliveries, if necessary.
    #echo "Uploading to BSZ"
    #upload_to_bsz_ftp_server.py "$ARCHIVE_FILE_MARC" /pub/UBTuebingen_Default/

    # Update contents of the timestamp file:
    #date --iso-8601=date > "$TIMESTAMP_FILE"
fi
