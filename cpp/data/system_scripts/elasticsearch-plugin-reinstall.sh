#!/bin/bash

output=$(needrestart -u NeedRestart::UI::stdio -r l -q -b 2>/dev/null)

if echo "$output" | grep -qE 'elasticsearch'; then
    echo "Detected elasticsearch restart requirement"
    plugin_name="analysis-icu"
    /usr/share/elasticsearch/bin/elasticsearch-plugin remove "$plugin_name"
    /usr/share/elasticsearch/bin/elasticsearch-plugin install "$plugin_name"
    exit 0
else
    echo "Elasticsearch either not installed or not upgraded. Nothing to do."
fi
