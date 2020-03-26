#!/bin/bash
# Tool for matching Krimdok local keywords to existing GND keywords
# GND keywords are obtained from LOBID
# LOBID_KEYWORD_DUMP can be obtained by
# curl --header "Accept-Encoding: gzip" "http://lobid.org/gnd/search?q=type:SubjectHeading&format=jsonl"
# LOBID_CORPORATE_DUMP can be obtained by
# curl --header "Accept-Encoding: gzip" "http://lobid.org/gnd/search?q=type:CorporateBody&format=jsonl"
WORKING_DIR="/tmp/localkeywords"
KRIMDOK_LOCAL_KEYWORD_FILE="local_keywords.txt"
KRIMDOK_NORMALIZED_KEYWORD_FILE="local_keywords_normalized.txt"

function GenerateLookupDB {
    # First line: Extract Terms and Synonyms and flatten to CSV
    # Second line: Generate a representation with a unique GND for each term
    # Third line: Convert CSV to TSV
    LOBID_FILE="$1"
    DB_FILENAME="$2"
    TMP_TSV=$(mktemp)
    zless ${LOBID_FILE} | jq -r ' [.gndIdentifier, .preferredName, .variantName] | flatten | @csv' \
    | awk -v FPAT="([^,]+)|(\"[^\"]+\")" '{for (column = 2; column <=NF; ++column) printf "%s, %s\n", $column, $1}' \
    | sed -E 's/("([^"]*)")?,/\2\t/g'  | sed -E 's/("([^"]*)")/\2/g' > ${TMP_TSV}

    kchashmgr create -otr ${DB_FILENAME}
    kchashmgr import ${DB_FILENAME} ${TMP_TSV}
    rm ${TMP_TSV}
}


function GenerateFullList {
    # The two sed expressions are needed to properly handle strings with single quotes pass
    LOCAL_KRIMDOK_KEYWORDS="$1"
    DB_FILENAME="$2"
    OUTFILE="$3"
    cat ${LOCAL_KRIMDOK_KEYWORDS} | sed -e "s/'/'\"'\"'/g" | awk '{ command=("bash -c  '\''kchashmgr get '"${DB_FILENAME}"'  \""$0"\" 2>/dev/null'\''");  if ((command | getline ppn) > 0) {  printf "%s : %s\n", $0, ppn } else { printf "%s :\n", $0;} close(command)}' | sed -e "s/'\"'\"'/'/" > ${OUTFILE}
}

function GenerateOnlyMatchingList {
     # The two sed expressions are needed to properly handle strings with single quotes pass
     LOCAL_KRIMDOK_KEYWORDS="$1"
     DB_FILENAME="$2"
     OUTFILE="$3"
     cat ${LOCAL_KRIMDOK_KEYWORDS} | sed -e "s/'/'\"'\"'/g" | awk '{ command=("bash -c  '\''kchashmgr get '"${DB_FILENAME}"'  \""$0"\" 2>/dev/null'\''");  if ((command | getline ppn) > 0) {  printf "%s : %s\n", $0, ppn } close(command)}' | sed -e "s/'\"'\"'/'/" > ${OUTFILE}
}

if [ $# != 3 ]; then
    echo "Usage: $0 krimdok_title LOBID_KEYWORD_DUMP.gz LOBID_CORPORATE_BODIES_DUMP.gz"
    exit 1
fi

KRIMDOK_TITLES=$(realpath "$1")
LOBID_KEYWORD_DUMP=$(realpath "$2")
LOBID_CORPORATE_BODIES_DUMP=$(realpath "$3")
KEYWORD_DB="LOBID_KEYWORD.db"
CORPORATE_BODIES_DB="LOBID_CORPORATE_BODIES.db"
KEYWORD_ONLY_MATCHING_FILE="keyword_only_matching.txt"
KEYWORD_ALL_FILE="keyword_all.txt"
CORPORATE_BODIES_ONLY_MATCHING_FILE="corporate_bodies_only_matching.txt"
CORPORATE_BODIES_ALL_FILE="corporate_bodies_all.txt"

mkdir --parents ${WORKING_DIR}
cd ${WORKING_DIR}
extract_local_krimdok_topics ${KRIMDOK_TITLES} ${KRIMDOK_LOCAL_KEYWORD_FILE}
#Trim whitespace
cat ${KRIMDOK_LOCAL_KEYWORD_FILE} | awk '{$1=$1};1' | sort | uniq > ${KRIMDOK_NORMALIZED_KEYWORD_FILE}

#Generate Lookup table and import to DB
GenerateLookupDB ${LOBID_KEYWORD_DUMP} ${KEYWORD_DB}
GenerateLookupDB ${LOBID_CORPORATE_BODIES_DUMP} ${CORPORATE_BODIES_DB}

# Generate the lists
GenerateOnlyMatchingList ${KRIMDOK_NORMALIZED_KEYWORD_FILE} ${KEYWORD_DB} ${KEYWORD_ONLY_MATCHING_FILE}
GenerateFullList ${KRIMDOK_NORMALIZED_KEYWORD_FILE} ${KEYWORD_DB} ${KEYWORD_ALL_FILE}

GenerateOnlyMatchingList ${KRIMDOK_NORMALIZED_KEYWORD_FILE} ${CORPORATE_BODIES_DB} ${CORPORATE_BODIES_ONLY_MATCHING_FILE}
GenerateFullList ${KRIMDOK_NORMALIZED_KEYWORD_FILE} ${CORPORATE_BODIES_DB} ${CORPORATE_BODIES_ALL_FILE}

# Join the matching result
# Consider that for matches in both files results from keywords is probably more appropriate
KEYWORD_CORPORATE_MATCHING_ONLY_FILE="keyword_corporate_matching_only.txt"
join -t ':' -a1 -a2 -21 <(sort ${KEYWORD_ONLY_MATCHING_FILE}) <(sort ${CORPORATE_BODIES_ONLY_MATCHING_FILE}) > ${KEYWORD_CORPORATE_MATCHING_ONLY_FILE}

cd -
