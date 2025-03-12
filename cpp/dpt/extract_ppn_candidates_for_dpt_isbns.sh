#!/bin/bash

if [ $# != 1 ]; then
    echo "Usage: $0 dpt_books_file.json"
    exit 1
fi


dpt_books_file="$1"

ids_and_isbns=$(cat ${dpt_books_file}  | \
    jq -r '.["Bücher"][] | [ .ID, ."ISBN-Print",  ."ISBN-eBook" ] | @csv' | \
    sed -re 's/[[:space:]"-]|[.]//g')

paste -d, <(echo "$ids_and_isbns" | cut -d',' -f 1) \
      <(echo "$ids_and_isbns" | cut -d',' -f 2 |
        xargs -I'{}' sh -c 'echo -n "$1,"; curl -s -X GET "https://krimdok.uni-tuebingen.de/api/v1/search?lookfor=isbn%3A"$1"%20%26%26%20format%3ABook&type=AllFields&sort=relevance%2C%20year%20desc&page=1&limit=20&prettyPrint=false&lng=en" -H "accept: application/json" \
        | jq -r '"'"' if .resultCount == 0 then [0, ""] else [ .resultCount, .records[].id ] end  | @csv '"'"' \
        ' _ '{}' | sed -re 's/"//g')

echo "-----------------------------------------"

ids_with_electronic_isbns=$(echo "$ids_and_isbns" | awk -F, '{if ($3=="") next; else print $0;}')
paste -d, <(echo "$ids_with_electronic_isbns" | cut -d',' -f 1) \
      <(echo "$ids_with_electronic_isbns" | cut -d',' -f 3 |
        xargs -I'{}' sh -c 'echo -n "$1,"; if [ -z "$1" ]; then echo "---"; exit 0; fi; curl -s -X GET "https://krimdok.uni-tuebingen.de/api/v1/search?lookfor=isbn%3A"$1"%20%26%26%20format%3ABook&type=AllFields&sort=relevance%2C%20year%20desc&page=1&limit=20&prettyPrint=false&lng=en" -H "accept: application/json" \
        | jq -r '"'"' if .resultCount == 0 then [0, ""] else [ .resultCount, .records[].id ] end  | @csv '"'"' \
        ' _ '{}' | sed -re 's/"//g')
