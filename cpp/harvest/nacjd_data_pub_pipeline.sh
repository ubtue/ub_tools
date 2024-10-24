#!/bin/bash
###########################################
# Author: Steven Lolong (steven.lolong@uni-tuebingen.de)
# Some contributions: Johannes Riedl (johannes.riedl@uni-tuebingen.de)
# copyright 2024 Tübingen University Library.  All rights reserved.
#
# Script to generate a nacjd data publication file
###########################################
set -o errexit -o nounset

if [ $# != 0 ]; then
    echo "Usage: $0"
    exit 1
fi

# please check the script 'extract_nacjd_json.sh' to produce NACJD_INPUT 
declare -r NACJD_INPUT="nacjd_$(date +%y%m%d).json"
declare -r NACJD_OUTPUT="nacjd_convert_$(date +%y%m%d).xml"
declare -r ISSN_NOT_FOUND_IN_K10_PLUS="not_found_or_print_version_in_k10plus_$(date +%y%m%d).txt"
declare -r STUDY_NUMBER_NOT_FOUND="study_number_not_found_$(date +%y%m%d).txt"
declare -r NACJD_OUTPUT_TRADITIONAL="nacjd_data_publication_traditional$(date +%y%m%d).txt"
declare -r ISSN_FILE="req_issn_$(date +%y%m%d).txt"
declare -r MARC_FILE_K10PLUS_WITH_DUPS="marc_from_k10plus_$(date +%y%m%d).mrc"
declare -r MARC_FILE_K10PLUS="marc_from_k10plus_no_dups_$(date +%y%m%d).mrc"
declare -r WORKING_DIR="/usr/local/ub_tools/cpp/harvest"
declare -r NACJD_TOOL="$WORKING_DIR/nacjd_data_publication"
declare -r AUGMENTING_787_TOOL="$WORKING_DIR/add_non_k10plus_787_information"
declare -r ISSN_LOOKUP_K10_PLUS_TOOL="$WORKING_DIR/issn_lookup.py"
declare -r NACJD_WITH_MISSING_SOME_STUDY_LINK="nacjd_data_incomplete_$(date +%y%m%d).xml"
declare -r ISSN_TO_BE_CONSIDERED="issn_to_be_considered_$(date +%y%m%d).txt"
declare -r ISSN_ALTERNATIVE_NEED_FROM_K10PLUS="alternative_issn_needed_to_be_download_from_k10plus_$(date +%y%m%d).txt"
declare -r BASE_OPENALEX_ISSN_API="https://api.openalex.org/sources/issn:"
declare -r ISSN_ALTERNATIVE_FROM_OPENALEX="info_issn_alternative_from_openalex_$(date +%y%m%d).json"
declare -r ISSN_ALTERNATIVE_FROM_OPENALEX_CSV="alternative_issn_openalex_$(date +%y%m%d).csv"
declare -r SOURCE_WITH_DUPS="alternative_issn_k10plus_$(date +%y%m%d).mrc"
declare -r SOURCE="alternative_issn_k10plus_no_dups_$(date +%y%m%d).mrc"
declare -r NOT_FOUND_ISSN="printed_or_not_found_$(date +%y%m%d).txt"
declare -r AUGMENTED_77w_OUTPUT="augmented_773w_$(date +%y%m%d).xml"
declare -r BASE_OPENALEX_DOI_API="https://api.openalex.org/works/https://doi.org/"
declare -r JSON_FROM_OPENALEX="open_access_info_from_openalex_$(date +%y%m%d).json"
declare -r NACJD_DOI="nacjd_dois_$(date +%y%m%d).txt"
declare -r OPEN_ACCESS_INFO_CSV="open_access_info_$(date +%y%m%d).csv"
declare -r CURRENT_KRIMDOK_FILE="GesamtTiteldaten-post-pipeline-240809.mrc"
declare -r EXISTING_STUDY_NUMBER_WITH_PPN="existing_study_number_and_ppn_$(date +%y%m%d).txt"
declare -r ISSN_FOR_GETTING_OPEN_ACCESS_INFO="issn_for_getting_open_access_info_$(date +%y%m%d).txt"
declare -r OPEN_ACCESS_INFO_ISSN_BASED_CSV="open_access_info_issn_based_$(date +%y%m%d).csv"
declare -r OLD_NACJD_MISSING_STUDIES_ID_TITLE_AUTHOR="old_nacjd_missing_id_title_author_$(date +%y%m%d).txt"
declare -r NACJD_STUDIES="nacjd_data_publication_update_studies_$(date +%y%m%d).xml"
declare -r NACJD_UPDATE_007_856="nacjd_data_publication_007_856$(date +%y%m%d).xml"
declare -r NACJD_FINAL="nacjd_data_publication_$(date +%y%m%d).xml"


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
cat "$NACJD_INPUT" | jq -r '.[].ISSN' | sort | uniq > "$ISSN_FILE"

echo "Downloading MARC from K10Plus"
$ISSN_LOOKUP_K10_PLUS_TOOL "$ISSN_FILE" "$MARC_FILE_K10PLUS_WITH_DUPS"

echo "Removing duplicatons in $MARC_FILE_K10PLUS_WITH_DUPS"
marc_remove_dups "$MARC_FILE_K10PLUS_WITH_DUPS" "$MARC_FILE_K10PLUS"

echo "Extracting control number and study number"
marc_grep $CURRENT_KRIMDOK_FILE '"LOK"' control_number_and_traditional |grep '[(]DE-2619[)]ICPSR' | awk -F':' '{print $1","$3}' | sed -re 's/  [$]0035  [$]a[(]DE-2619[)]ICPSR//' > "$EXISTING_STUDY_NUMBER_WITH_PPN"

echo "Augmenting MARC using info from K10Plus"
$NACJD_TOOL "--verbose" "convert" $NACJD_INPUT $MARC_FILE_K10PLUS $EXISTING_STUDY_NUMBER_WITH_PPN $ISSN_NOT_FOUND_IN_K10_PLUS $STUDY_NUMBER_NOT_FOUND $NACJD_OUTPUT

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
cat "$NACJD_INPUT" | jq -r '.[].DOI' | sort | uniq > "$NACJD_DOI"

echo "Removing space from $NACJD_DOI"
sed -i "s/ //g" $NACJD_DOI

echo "Downloading Open Access Info from Openalex"
echo "Creating/ Cleaning: $JSON_FROM_OPENALEX"
echo "" > $JSON_FROM_OPENALEX
echo "Starting download data"
while read DOI; do
    echo $DOI
    curl  -s "$BASE_OPENALEX_DOI_API$DOI?select=doi,open_access" >> $JSON_FROM_OPENALEX
done < $NACJD_DOI 

echo "Removing error tag from $JSON_FROM_OPENALEX" 
remove_error_message $JSON_FROM_OPENALEX

echo "Creating open access info CSV: $OPEN_ACCESS_INFO_CSV" 
cat $JSON_FROM_OPENALEX |jq -r '[.doi, .open_access.is_oa, .open_access.any_repository_has_fulltext] | flatten | @csv' > $OPEN_ACCESS_INFO_CSV

echo "Extracting ISSN for getting open access info"
marc_grep $AUGMENTED_77w_OUTPUT '"773x"' traditional | sed -re 's/773 //' | sort | uniq > $ISSN_FOR_GETTING_OPEN_ACCESS_INFO

echo "Getting open access information based on ISSN"
cat $ISSN_FOR_GETTING_OPEN_ACCESS_INFO | xargs -I'{}' sh -c 'echo "$@" $(curl -L -s https://api.openalex.org/sources/issn:"$@" |jq -r .is_oa)' _ '{}' > $OPEN_ACCESS_INFO_ISSN_BASED_CSV

echo "Updating open access info"
$NACJD_TOOL "--verbose" "augment_open_access" $AUGMENTED_77w_OUTPUT $OPEN_ACCESS_INFO_CSV $OPEN_ACCESS_INFO_ISSN_BASED_CSV $NACJD_WITH_MISSING_SOME_STUDY_LINK 

echo "Downloading NACJD studies referenced  were missing from the old approach"
time cat $STUDY_NUMBER_NOT_FOUND | xargs -I '{}' sh -c 'curl -s https://pcms.icpsr.umich.edu/pcms/api/1.0/studies/$@ > $@.json' _ '{}'

echo "Extracting downloaded information"
ls -1 *.json | xargs -I'{}' sh -c 'echo ${@%%.json}\\t$(cat $@ | jq -r -C .projectTitle)\\t$(cat $@ | jq -r '"'"'.creators | map("\(.orgName), \(.personName)") | join("; ")'"'"')' _ '{}' | sed -re 's/([, ]+)?null([, ]+)?//g' > $OLD_NACJD_MISSING_STUDIES_ID_TITLE_AUTHOR

echo "Adding information about studies, that are not in K10Plus to 787"
$AUGMENTING_787_TOOL  $NACJD_WITH_MISSING_SOME_STUDY_LINK $OLD_NACJD_MISSING_STUDIES_ID_TITLE_AUTHOR  $NACJD_STUDIES

echo "List the ISSNs to be considered"
$NACJD_TOOL "--verbose" "suggested_report" $NOT_FOUND_ISSN $SOURCE $ISSN_TO_BE_CONSIDERED

# If field 856u is present in the record, it means that the record is an online version, so it is necessary to update the information in field 007 from print to online.
# The field 787 contains information about related research with the record. Therefore, when the 787t is present, it is needed to add subfield i:Forschungsdaten.
echo "Update 007 to online when online information exists in 856u." 
echo "Add 787i:Forschungsdaten when 787t is present."
marc_augmentor $NACJD_STUDIES $NACJD_UPDATE_007_856 --replace-field-if '007:cr|||||' '856u:\W+' --add-subfield-if '787i:Forschungsdaten' '787t:\W+'

# When field 773 is missing and the record type is an article, the assumption is that the record should be a monograph. In this case, the leader annotation must be changed from article to book. 
# Otherwise, when field 773 exists, and the record type is a book, the assumption is that the record should be an article. In this case, the leader annotation must be updated from book to article. 
echo "Update leader to a monograph when the 773 is not present."
$NACJD_TOOL "--verbose" "update_monograph" $NACJD_UPDATE_007_856 $NACJD_FINAL

