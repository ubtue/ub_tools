#!/bin/bash
set -o errexit -o nounset -o pipefail

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

markdown_converter="python3 convert_pdf_to_md.py"
kimi_script="./analecta_hermeneutika_kimi.sh"

for folder in volume*/; do
  json_file="${folder%/}/${folder%/}.json"

  if [[ -f "$json_file" ]]; then
    inner_json=$(jq '.choices[].message.content' "$json_file" | \
    awk -F '```json|```' '{print $2}' | \
    sed -re 's/\\[nt]//g' | grep -v '^$' | \
    sed -re 's/(.*)/"\1"/' | jq -ra fromjson)

    echo "$inner_json" | jq -c '.articles[]' | while read -r article; do
      pdf_url=$(echo "$article" | jq -r '.url')
      pdf_name=$(basename "$pdf_url")

      curl -s -o "${tmpdir}/$pdf_name" "$pdf_url"

      $markdown_converter "${tmpdir}/$pdf_name" "${tmpdir}/${pdf_name%.pdf}.md"

      md_file="${tmpdir}/${pdf_name%.pdf}.md"

      time $kimi_script "$md_file" > "${folder%/}/${pdf_name%.pdf}.json"

      echo "Processed $pdf_name and saved JSON in $folder"
    done
  else
    echo "JSON file not found in $folder"
  fi
done
