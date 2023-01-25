#!/bin/bash
set -o errexit -o nounset

if [[ $# != 2 ]]; then
    echo "Usage: $0 krim_keywords.mrc gnd_full_dump_mrc"
    exit 1
fi


krim_extract_commands=$(mktemp /tmp/krim_extract_commands_XXX.sh)
chmod 755 ${krim_extract_commands}

function CleanUp {
    rm ${krim_extract_commands}
}

trap CleanUp EXIT

krim_keywords="$1"
export gnd_full_dump="$2"


marc_grep ${krim_keywords} '"024a"' traditional | awk  '{print $2}' | 
    xargs  -I'{}' -n 1000 /bin/bash -c  $'echo marc_grep "${gnd_full_dump}" \\\'if \'"024a"=="\'$(echo \("$@"\) | tr " " \'|\' | sed -r -e \'s/[|]$//\')\'"\'extract *\\\' marc_binary' '{}' > ${krim_extract_commands}

${krim_extract_commands}


