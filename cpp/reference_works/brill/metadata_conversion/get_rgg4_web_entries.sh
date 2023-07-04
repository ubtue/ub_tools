#!/bin/bash
set -o errexit -o nounset


function RemoveTempFiles {
    rm ${tmpfile1}
}

#trap RemoveTempFiles EXIT


BASE_URL='https://referenceworks.brillonline.com/browse/religion-in-geschichte-und-gegenwart/'

function GetPage {
    local url="$1"
    curl --silent --show-error ${url}
}


function GetLinks {
    local url="$1"
    local filter="$2"
    #c.f. https://superuser.com/questions/372155/how-do-i-extract-all-the-external-links-of-a-web-page-and-save-them-to-a-file (230223)
    curl --silent --show-error ${url}  \
        | xmllint --html --xpath  '//a[starts-with(@href, "http")]/@href' 2>/dev/null - \
        | sed -re 's/^ href="|"$//g' \
        | grep "${filter}"
}


function GetRangePages {
    local outfile="$1"
    local links=($(GetLinks ${BASE_URL} "/alpha/"))
    local let sublinks=()
    for link in "${links[@]}"; do
        sublinks+=($(GetLinks ${link} "/alphaRange/"))
    done
    
    > ${outfile}
    for link in "${sublinks[@]}"; do
        echo ${link}
        echo ${link} >> ${outfile}
    done
}


function GetNextLink {
    local url="$1"
    local xpath=$(printf "%s" '//a[contains(.,'"'"'Next'"'"')]/@href')
    curl --silent --show-error ${url} \
        | xmllint --html --xpath "${xpath}"  2>/dev/null - \
        | sed -re 's/^ href="|"$//g' \
        | sed -re 's/\&amp;/\&/g'
}


function AppendEntriesToOutfile {
   local url="$1"
   local outfile="$2"
   ./translate_url "${url}" | jq --raw-output '.items | to_entries[] | .value' >> ${outfile}
}

tmpfile1=$(mktemp -t rgg4XXXXX.txt)

if [[ $# != 1 ]]; then
    echo "Usage: $0 outfile"
    exit 1
fi


outfile="$1"
echo "Get Page Ranges..."
GetRangePages ${tmpfile1}
echo "Finished Getting the Page Ranges"
> ${outfile}
echo "Obtaining individual record..."
for rangePage in $(cat "${tmpfile1}"); do
    echo ${rangePage}
    url="${rangePage}"'?s.rows=100'
    AppendEntriesToOutfile "${url}" "${outfile}"
    nextPageInRange=$(GetNextLink ${url})
    while [ -n "${nextPageInRange}" ]; do
        echo ${nextPageInRange}
        AppendEntriesToOutfile "${nextPageInRange}" "${outfile}"
        nextPageInRange=$(GetNextLink "${nextPageInRange}")
    done
done
echo "Done..."
