#/bin/bash
# Some cleanup operations as leftover from the generation
set -o nounset -o pipefail

function Usage {
    echo "Usage $0 marc_in marc_out bibwiss1.csv bibwiss2.csv ..."
    exit 1
}


NUM_OF_TMPFILES=3
function GenerateTmpFiles {
    for i in $(seq 1 ${NUM_OF_TMPFILES}); do
        tmpfiles+=($(mktemp --tmpdir $(basename -- ${marc_out%.*}).XXXX.${marc_out##*.}));
    done
}


function CleanUpTmpFiles {
   for tmpfile in ${tmpfiles[@]}; do rm ${tmpfile}; done
}


function AddToLookupTable {
    key="$1"
    shift
    value="$@"
    echo [${key}]=\"${value}\"
}

if [[ $# < 3 ]]; then
   Usage
fi

marc_in="$1"
shift
marc_out="$1"
shift


# Some of the entries in the DB were not really present in the web interface
tmpfiles=()
GenerateTmpFiles
echo ${tmpfiles[@]}
invalid_urls=($(diff --new-line-format="" --unchanged-line-format="" <(marc_grep ${marc_in} '"856u"' traditional | awk -F' ' '{print $2}' | sort) \
    <(cat $@ | csvcut --column 2 | grep '^https' | sed -r -e 's#/$##' | sort)))
marc_filter ${marc_in} ${tmpfiles[0]}  --drop '856u:('$(IFS=\|; echo "${invalid_urls[*]}")')'

# Do some cleanup for entries that are apparently wrong"
marc_filter ${tmpfiles[0]} ${tmpfiles[1]} --replace "100a:700a:" "^sf,\s+(Mirjam)\s+(Schambeck)" "\\2, \\1"
marc_augmentor ${tmpfiles[1]} ${tmpfiles[2]} --add-subfield-if-matching  '1000:(DE-588)120653184' '100a:Schambeck, Mirjam'

# References need a year and a hint to which other keyword they link
declare -A references
export -f AddToLookupTable
eval "references_content=\$(cat \$@ | csvcut -C 2 | csvgrep --column 1 -m \"Reference\" | csvcut -C 1 | csvtool call AddToLookupTable -)"
eval "references=($(echo ${references_content}))"


echo ${references[@]}
echo ${references[Zweiter Jesaja]}


CleanUpTmpFiles
