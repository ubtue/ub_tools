#/bin/bash
# Some cleanup operations as leftover from the generation

function Usage {
    echo "Usage $0 marc_in marc_out bibwiss1.csv bibwiss2.csv ..."
    exit 1
}



if [[ $# < 3 ]]; then
   Usage
fi

marc_in="$1"
shift
marc_out="$1"
shift

# Some of the entries in the DB were not really present in the web interface
invalid_urls=($(diff --new-line-format="" --unchanged-line-format="" <(marc_grep ${marc_in} '"856u"' traditional | awk -F' ' '{print $2}' | sort) \
    <(cat $@ | csvcut --column 2 | grep '^https' | sed -r -e 's#/$##' | sort)))
marc_filter "${marc_in}" "${marc_out}"  --drop '856u:('$(IFS=\|; echo "${invalid_urls[*]}")')'
