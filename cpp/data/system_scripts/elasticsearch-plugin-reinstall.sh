#!/bin/bash

found=false

while IFS= read -r line; do
    if [[ $line == elasticsearch* ]]; then
        echo "Detected elasticsearch line: $line"
        plugin_name="analysis-icu"
        echo "Removing plugin: $plugin_name"
        sudo /usr/share/elasticsearch/bin/elasticsearch-plugin remove "$plugin_name" || true
        echo "Installing plugin: $plugin_name"
        sudo /usr/share/elasticsearch/bin/elasticsearch-plugin install "$plugin_name" --batch
        found=true
        break
    fi
done

if [ "$found" = false ]; then
    echo "Elasticsearch either not installed or not upgraded. Nothing to do."
fi
