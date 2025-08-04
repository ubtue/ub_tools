#!/bin/bash

if [ $# != 1 ]; then
    echo "Usage: $0 mardown_with_pages.md"
    exit 1
fi

markdown_file="$1"

echo -n "$(basename ${markdown_file%%.*}) "
echo $(cat ${markdown_file} | egrep '^[[:space:]]*[[:digit:]]+[[:space:]]*$' | sed -n '1p;$p' |  tr -d ' ' | paste -sd '-')



