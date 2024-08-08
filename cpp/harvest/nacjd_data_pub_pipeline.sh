#!/bin/bash
set -o errexit -o nounset

if [ $# != 0 ]; then
    echo "Usage: $0"
    exit 1
fi

declare -r NACJD_INPUT="nacjd.json"
declare -r NACJD_OUTPUT="nacjd_convert_$(date +%y%m%d).xml"
declare -r ISSN_NOT_FOUND_IN_K10_PLUS="not_found_or_print_version_in_k10plus_$(date +%y%m%d).txt"
declare -r NACJD_OUTPUT_TRADITIONAL="nacjd_data_publication_traditional$(date +%y%m%d).txt"
declare -r ISSN_FILE="req_issn.txt"
declare -r MARC_FILE_K10PLUS_WITH_DUPS="marc_from_k10plus.mrc"
declare -r MARC_FILE_K10PLUS="marc_from_k10plus_no_dups.mrc"
declare -r WORKING_DIR="/usr/local/ub_tools/cpp/harvest/"
declare -r NACJD_TOOL="$WORKING_DIR/nacjd_data_publication"
declare -r ISSN_LOOKUP_K10_PLUS_TOOL="$WORKING_DIR/issn_lookup.py"
declare -r FINAL_OUTPUT="nacjd_data_publication_$(date +%y%m%d).xml"
declare -r ISSN_TO_BE_CONSIDER="issn_to_be_considered_$(date +%y%m%d).xml"
declare -r ISSN_ALTERNATIVE_NEED_FROM_K10PLUS="alternative_issn_needed_to_be_download_from_k10plus_$(date +%y%m%d).txt"
declare -r BASE_OPENALEX_ISSN_API="https://api.openalex.org/sources/issn:"
declare -r ISSN_ALTERNATIVE_FROM_OPENALEX="info_issn_alternative_from_openalex_$(date +%y%m%d).json"
declare -r ISSN_ALTERNATIVE_FROM_OPENALEX_CSV="alternative_issn_openalex_$(date +%y%m%d).csv"
declare -r SOURCE_WITH_DUPS="alternative_issn_k10plus.mrc"
declare -r SOURCE="alternative_issn_k10plus_no_dups.mrc"
declare -r NOT_FOUND_ISSN="printed_or_not_found_$(date +%y%m%d).txt"
declare -r AUGMENTED_77w_OUTPUT="augmented_773w_$(date +%y%m%d).xml"
declare -r BASE_OPENALEX_DOI_API="https://api.openalex.org/works/https://doi.org/"
declare -r JSON_FROM_OPENALEX="open_access_info_from_openalex.json"
declare -r NACJD_DOI="nacjd_dois_$(date +%y%m%d).txt"
declare -r OPEN_ACCESS_INFO_CSV="open_access_info_$(date +%y%m%d).csv"

remove_error_message(){
    FILE_NAME=$1
    echo "Removing unwanted information (error message) in the file $FILE_NAME"
    sed -i '/doctype html/d' $FILE_NAME  
    sed -i '/html lang=en/d' $FILE_NAME 
    sed -i '/404 Not Found/d' $FILE_NAME 
    sed -i '/The requested URL was not found/d' $FILE_NAME 
    sed -i '/Not Found/d' $FILE_NAME 
    sed -i "s/ //g" $FILE_NAME
}


echo "Extracting ISSN"
cat "$NACJD_INPUT" | jq -r '.searchResults.response.docs[].ISSN' | sort | uniq > "$ISSN_FILE"

echo "Downloading MARC from K10Plus"
$ISSN_LOOKUP_K10_PLUS_TOOL "$ISSN_FILE" "$MARC_FILE_K10PLUS_WITH_DUPS"

echo "Removing duplicatons in $MARC_FILE_K10PLUS_WITH_DUPS"
marc_remove_dups "$MARC_FILE_K10PLUS_WITH_DUPS" "$MARC_FILE_K10PLUS"

echo "Augmenting MARC using info from K10Plus"
$NACJD_TOOL "--verbose" "convert" $NACJD_INPUT $MARC_FILE_K10PLUS $ISSN_NOT_FOUND_IN_K10_PLUS $NACJD_OUTPUT

echo "Downloading alternative ISSN from openalex"
echo "Creating/ Cleaning: $ISSN_ALTERNATIVE_FROM_OPENALEX"
echo "" > $ISSN_ALTERNATIVE_FROM_OPENALEX
echo "Starting download data"
while read ISSN; do
    echo "$BASE_OPENALEX_ISSN_API$ISSN?select=issn_l,issn"
    curl -L "$BASE_OPENALEX_ISSN_API$ISSN?select=issn_l,issn" >> $ISSN_ALTERNATIVE_FROM_OPENALEX
done < $ISSN_NOT_FOUND_IN_K10_PLUS

echo "Removing error in  $ISSN_ALTERNATIVE_FROM_OPENALEX"
remove_error_message $ISSN_ALTERNATIVE_FROM_OPENALEX

cat $ISSN_ALTERNATIVE_FROM_OPENALEX |jq -r '.issn[]' > $ISSN_ALTERNATIVE_NEED_FROM_K10PLUS

echo "Downloading Alternative MARC from K10Plus"
$ISSN_LOOKUP_K10_PLUS_TOOL "$ISSN_ALTERNATIVE_NEED_FROM_K10PLUS" "$SOURCE_WITH_DUPS"

echo "Removing duplicatons"
marc_remove_dups "$SOURCE_WITH_DUPS" "$SOURCE"

echo "Creating $ISSN_ALTERNATIVE_FROM_OPENALEX_CSV"
cat $ISSN_ALTERNATIVE_FROM_OPENALEX |jq -r '[.issn_l, .issn[]] | flatten | @csv' > $ISSN_ALTERNATIVE_FROM_OPENALEX_CSV

echo "Augmenting 773w"
$NACJD_TOOL "--verbose" "augment_773w" $NACJD_OUTPUT $ISSN_ALTERNATIVE_FROM_OPENALEX_CSV $SOURCE $NOT_FOUND_ISSN $AUGMENTED_77w_OUTPUT


echo "Extracting DOI"
cat "$NACJD_INPUT" | jq -r '.searchResults.response.docs[].DOI' | sort | uniq > "$NACJD_DOI"

echo "Removing space from $NACJD_DOI"
sed -i "s/ //g" $NACJD_DOI

echo "Downloading Open Access Info from Openalex"
echo "Creating/ Cleaning: $JSON_FROM_OPENALEX"
echo "" > $JSON_FROM_OPENALEX
echo "Starting download data"
while read DOI; do
    echo $DOI
    curl "$BASE_OPENALEX_DOI_API$DOI?select=doi,open_access" >> $JSON_FROM_OPENALEX
done < $NACJD_DOI 

echo "Removing error tag from $JSON_FROM_OPENALEX" 
remove_error_message $JSON_FROM_OPENALEX

echo "Creating open access info CSV: $OPEN_ACCESS_INFO_CSV" 
cat $JSON_FROM_OPENALEX |jq -r '[.doi, .open_access.is_oa, .open_access.any_repository_has_fulltext] | flatten | @csv' > $OPEN_ACCESS_INFO_CSV

echo "Updating open access info"
$NACJD_TOOL "--verbose" "augment_open_access" $AUGMENTED_77w_OUTPUT $OPEN_ACCESS_INFO_CSV $FINAL_OUTPUT 

echo "List the ISSNs to be considered"
$NACJD_TOOL "--verbose" "suggested_report" $NOT_FOUND_ISSN $SOURCE 