#!/bin/bash

volume_file="hermeneutica_volumes.txt"
kimi_script="./analecta_hermeneutika_kimi_overview_data.sh"

while IFS= read -r line
do
  if [[ $line == Link:* ]]; then
    url="${line#Link: }"
    folder=$(basename "$url")
    mkdir -p "$folder"
    time $kimi_script "$url" > "$folder/$folder.json"
    echo "Saved JSON for $url to $folder/$folder.json"
  fi
done < "$volume_file"
