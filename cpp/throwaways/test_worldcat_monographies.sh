#!/bin/bash
set -o errexit -o nounset

all_oclc_param="--get-all-oclc-numbers"

if [[ $# < 2 ]]; then
   echo "Usage: $0 [${all_oclc_param}] oclc_credentials_file isbn|isbn_file"
   exit 1
fi

get_all_oclc_numbers=false

if [ "$1" = ${all_oclc_param} ]; then
    get_all_oclc_numbers=true
    shift
fi


credentials_file="$1"

function GetAccessToken {
    key=$(inifile_lookup ${credentials_file} Credentials key)
    secret=$(inifile_lookup ${credentials_file} Credentials secret)
    id=$(inifile_lookup ${credentials_file} Credentials id)
    echo $(curl -s -u "${key}:${secret}" -X POST -H 'Accept: application/json' 'https://oauth.oclc.org/token?grant_type=client_credentials&scope=wcapi%20context:'"${id}" | jq --raw-output .access_token)
}

function QueryISBN {
    local isbn="$1"
    echo $(curl -s -S "https://americas.discovery.api.oclc.org/worldcat/search/v2/bibs?q=bn%3A%20${isbn}&itemType=&itemSubType=&retentionCommitments=false&facets=creator&groupRelatedEditions=false&groupVariantRecords=false&orderBy=bestMatch" -H 'accept: application/json' -H "Authorization: Bearer ${access_token}")
}


function GetFirstOCLCNumber {
   result=$(QueryISBN ${isbn})
   if [ $(echo ${result} | jq --raw-output .message) = "Unauthorized" ]; then
       access_token=$(GetAccessToken)
       result=$(QueryISBN ${isbn})
   fi
   if ${get_all_oclc_numbers}; then
      echo ${result} | jq --raw-output '.bibRecords | .[].identifier.oclcNumber'
   else
      echo ${result} | jq --raw-output '.bibRecords | .[0].identifier.oclcNumber'
   fi
}


function PrintCSVResult {
   local isbn="$1"
   echo ${isbn},$(GetFirstOCLCNumber ${isbn})
}

access_token=$(GetAccessToken)
if [ -r "$2" ]; then
    echo "Entering file mode"
    cat "$2" | while read isbn; do
       PrintCSVResult ${isbn}
    done
else
   echo "Entering single isbn mode"
   isbn="$2"
   PrintCSVResult ${isbn}
fi



