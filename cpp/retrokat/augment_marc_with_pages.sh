#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Usage: $0 marc.xml url_to_pages.json"
    exit 1
fi

marc_file="$1"
json_file="$2"
output_file="ixtheo_zotero_$(date +%Y%m%d)_001.xml"

tmp_augmented="tmp_augmented.xml"
tmp_final="tmp_final.xml"

augment_args=""

while IFS= read -r url; do
    page_range=$(jq -r --arg url "$url" '.[$url]' "$json_file")

    if [ -n "$page_range" ] && [ "$page_range" != "null" ]; then
        echo "Adding augment arg for URL: $url with page range: $page_range"
        augment_args+="--add-subfield-if '936h:$page_range' '856u:$url' "
    else
        echo "Warning: No page range found for URL: $url"
    fi
done < <(jq -r 'keys[]' "$json_file")

eval marc_augmentor "$marc_file" "$tmp_augmented" $augment_args

cp "$tmp_augmented" "$tmp_final"

while IFS= read -r url; do
    page_range=$(jq -r --arg url "$url" '.[$url]' "$json_file")

    if [ -n "$page_range" ] && [ "$page_range" != "null" ]; then
        echo "Updating 773g for URL: $url with page range: $page_range"
        current_value=$(xmlstarlet sel -N marcnsp="http://www.loc.gov/MARC21/slim" -t -v "//marcnsp:record[marcnsp:datafield[@tag='856']/marcnsp:subfield[@code='u' and text()='$url']]/marcnsp:datafield[@tag='773']/marcnsp:subfield[@code='g']" "$tmp_final")
        echo "Current 773g: '$current_value'"
        if [ -z "$current_value" ]; then
            echo "Warning: 773g field not found for URL: $url"
            continue
        fi

        xmlstarlet ed -L -N marcnsp="http://www.loc.gov/MARC21/slim" \
          -u "//marcnsp:record[marcnsp:datafield[@tag='856']/marcnsp:subfield[@code='u' and text()='$url']]/marcnsp:datafield[@tag='773']/marcnsp:subfield[@code='g']" \
          -x "concat(., ', Seite $page_range')" \
          "$tmp_final"
    else
        echo "Warning: 773g adjustment skipped; No page range for URL: $url"
    fi
done < <(jq -r 'keys[]' "$json_file")

mv "$tmp_final" "$output_file"
rm -f "$tmp_augmented"

echo "Output written to $output_file"
