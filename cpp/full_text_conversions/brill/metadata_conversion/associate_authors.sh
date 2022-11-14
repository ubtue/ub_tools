#!/bin/bash
set -o errexit -o nounset -o pipefail

if [ $# != 2 ]; then
    echo "usage: $0 input_marc associated.txt"
    exit 1
fi

input_file="$1"
association_file="$2"
>${association_file}

for tag in "100" "700"; do
    marc_grep ${input_file} 'if "001"==".*" extract "'${tag}'a"' traditional |
    sed -e 's/^[17]00 //' | \
    `# Remove leading and trailing whitespace` \
    sed -e 's/^[[:space:]]*//; s/[[:space:]]*$//;' | \
    sort | \
    uniq | \
    tr '\n' '\0' | \
    xargs -0 -I'{}' sh -c 'echo "$@" "'"|"'" $(swb_author_lookup "$@") ' _ '{}' | \
    `# Filter empty results` \
    grep -v '[|]\s*$' \
    >> ${association_file}
done

