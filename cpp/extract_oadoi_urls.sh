#!/bin/bash
# Extract URLs for Unpaywall database for further processing
set -o errexit -o nounset

DOI_FILE="$1"
FINAL_OUTPUT_FILE="$2"
MONGO_COMMAND_FILE="mongo_chunk_query.js"
INTERMEDIATE_OUTPUT_FILE="oadoi_intermediate.json"
CHUNK_SIZE=10000
SPLIT_FILE_PREFIX="__x"

# Generate query for this chunk
function GenerateMongoQueryFile {
    # From a given chunk file generate a comma separated list of DOI's and extract existing best_oa_location 
    # i.e. quote the single lines and join them to a comma separated array first and then extract the results set 
    # of this DOI's ANDed with non-null best_oa_location
    cat "$1" | sed -e 's/^/"/; s/$/" /' | sed -re ':a;N;$!ba;s/\n/, /g' |  sed '1i db.all_oadoi.find( { $and: [ { "doi" : { $in:  [' - | sed -e '$a] }}, { best_oa_location : { $ne : null } } ] }, { "doi": 1, "best_oa_location" : 1, "_id": 0 }).forEach(doc => print(JSON.stringify(doc)));' > ${MONGO_COMMAND_FILE}
}


function AppendOADOIChunk {
    mongosh --quiet oadoi "$1" >> "$2"
}


function CleanUp {
    rm ${SPLIT_FILE_PREFIX}*
    rm ${INTERMEDIATE_OUTPUT_FILE}  
    rm ${MONGO_COMMAND_FILE}
}


# Generate chunks from original file
split -l ${CHUNK_SIZE} ${DOI_FILE} ${SPLIT_FILE_PREFIX}

# Flush output file
> ${INTERMEDIATE_OUTPUT_FILE}

# Append each chunk to final file
for DOI_CHUNK_FILE in $(ls ${SPLIT_FILE_PREFIX}*)
do
   GenerateMongoQueryFile ${DOI_CHUNK_FILE}
   AppendOADOIChunk ${MONGO_COMMAND_FILE} ${INTERMEDIATE_OUTPUT_FILE}
done;

# Make final output an array
cat ${INTERMEDIATE_OUTPUT_FILE} | jq --slurp '.' > ${FINAL_OUTPUT_FILE}

CleanUp
