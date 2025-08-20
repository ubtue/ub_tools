#!/bin/bash
set -e


host_and_port=$(inifile_lookup /usr/local/var/lib/tuelib/Elasticsearch.conf Elasticsearch host)


# Create the indices and types:
for schema in *_schema.json; do
    index="${schema%_schema.json}"
    curl --fail --request PUT --header 'Content-Type: application/json' "${host_and_port}/${index}" --data @"$schema"
    echo
done
