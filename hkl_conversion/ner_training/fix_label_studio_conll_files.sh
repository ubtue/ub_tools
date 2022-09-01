#!/bin/bash
#Conll export from label studio misses an additional 'O' at DOCSTART for the spacy convert function

if [ $# != 1 ]; then
    echo "Usage: $0 conll_dir"
    exit 1
fi

conll_dir="$1"

for file in $(ls --directory -1 "${conll_dir}"/*); do
    echo "Fixing ${file}..."
    sed --in-place --regexp-extended  's/^(-DOCSTART- -X- O)$/\1 O/' "${file}"
done
