#!/bin/bash

while IFS= read -r line; do
    if [[ $line == elasticsearch* ]]; then
        echo "Detected elasticsearch line: $line"
        plugin_name="analysis-icu"
        echo "Removing plugin: $plugin_name"
        /usr/share/elasticsearch/bin/elasticsearch-plugin remove "$plugin_name"
        echo "Installing plugin: $plugin_name"
        /usr/share/elasticsearch/bin/elasticsearch-plugin install "$plugin_name" --batch
        exit 0;
    fi
done

echo "Elasticsearch either not installed or not upgraded. Nothing to do."
