#/bin/bash
# Some cleanup operations as leftover from the generation
set -o nounset -o pipefail

function Usage {
    echo "Usage $0 marc_in marc_out bibwiss1.csv bibwiss2.csv ..."
    exit 1
}


NUM_OF_TMPFILES=4
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


function AddToLookupTableSwap {
    key="$1"
    shift
    value="$@"
    echo [${value}]=\"${key}\"
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
invalid_urls=($(diff --new-line-format="" --unchanged-line-format="" <(marc_grep ${marc_in} '"856u"' traditional | awk -F' ' '{print $2}' | sort) \
    <(cat $@ | csvcut --column 2 | grep '^https' | sed -r -e 's#/$##' | sort)))
marc_filter ${marc_in} ${tmpfiles[0]}  --drop '856u:('$(IFS=\|; echo "${invalid_urls[*]}")')'

# Do some cleanup for entries that are apparently wrong"
marc_filter ${tmpfiles[0]} ${tmpfiles[1]} --replace "100a:700a:" "^sf,\s+(Mirjam)\s+(Schambeck)" "\\2, \\1"
marc_augmentor ${tmpfiles[1]} ${tmpfiles[2]} --add-subfield-if-matching  '1000:(DE-588)120653184' '100a:Schambeck, Mirjam'

# References need a year and a hint to which other keyword they link
declare -A references
export -f AddToLookupTable
eval "references_content=\$(cat \$@ | csvcut --not-columns 2 | grep '^Reference' | csvcut --not-columns 1 | csvtool call AddToLookupTable -)"
eval "references=($(echo ${references_content}))"

declare -A ppns_and_titles
declare -A titles_and_ppns
declare -A ppns_and_years
export -f AddToLookupTableSwap
eval "ppns_and_titles=($(marc_grep ${tmpfiles[2]} '"245a"' control_number_and_traditional | csvcut -d: -c 1,3 | csvtool call AddToLookupTable -))"
eval "titles_and_ppns=($(marc_grep ${tmpfiles[2]} '"245a"' control_number_and_traditional | csvcut -d: -c 1,3 | csvtool call AddToLookupTableSwap -))"
eval "ppns_and_years=($(marc_grep ${tmpfiles[2]} '"936j"' control_number_and_traditional | csvcut -d: -c 1,3 | csvtool call AddToLookupTable -))"
arguments="marc_augmentor ${tmpfiles[2]} ${tmpfiles[3]} "
for record_ppn in "${!ppns_and_titles[@]}"; do
    record_title="${ppns_and_titles[${record_ppn}]}"
    if [[ -v references[${record_title}] ]]; then
        reference_title="${references[${record_title}]:-}"
        arguments+=" --insert-field-if '500a:Verweis auf \"${reference_title}\"' '001:${record_ppn}'"
        reference_ppn="${titles_and_ppns[${reference_title}]:-BIB00000000}"
        reference_year="${ppns_and_years[${reference_ppn}]:-2005}"
        arguments+=" --insert-field-if '264c:${reference_year}' '001:${record_ppn}' \
                     --insert-field-if '936j:${reference_year}' '001:${record_ppn}'"

    fi
done
eval "${arguments}"

# Make sure we do not have " / " - this means the separation of an author
marc_augmentor ${tmpfiles[3]} ${marc_out} --replace-subfield-if-regex '245a:/\s+\/\s+/\//g' '245a:\s+/\s+' \
                                          --replace-subfield-if-regex '500a:/\s+\/\s+/\//g' '500a:\s+/\s+'

CleanUpTmpFiles
