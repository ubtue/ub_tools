#!/bin/bash
# Extract data from NACJD result page 
# Do e.g. a 'wget -O nacjd_raw_data.html https://www.icpsr.umich.edu/web/NACJD/search/publications?start=0&sort=TITLE_SORT%20asc&ARCHIVE=NACJD&PUBLISH_STATUS=PUBLISHED&rows=50000' to a file an pass it as argument
set -o errexit -o nounset -o pipefail

if [ $# != 1 ]; then
    echo "Usage: $0 nacjd_html"
    exit 1
fi


nacjd_full_records_page="$1"

# Extract the JS-script with the embedded JSON data
# generate an AST, extract the relevant JSON part 
# then convert it back
# Execute npm i -g shift-parser and npm i -g shift-codegen and
# npm link shift-parser and npm link shift-codegen in the directory
# of this script if needed

tidy --error --quiet -ashtml ${nacjd_full_records_page} 2>/dev/null | \
xmllint --format --html --huge --xpath \
   "//script[contains(text(), 'searchResults')]" - | \
xmllint --huge --nocdata --xpath "//script/text()" - | \
./generate-shift-parser-ast.js | \
jq '.tree.statements[] | .. |  select(type == "object" and .type == "DataProperty" and .name.value == "docs") ' | \
./generate-code-from-shift-ast.js 
