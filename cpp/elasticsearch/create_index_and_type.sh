#!/bin/bash
set -e


CONFIG_FILE=/usr/local/var/lib/tuelib/Elasticsearch.conf

host_and_port=$(inifile_lookup $CONFIG_FILE Elasticsearch host)
index=$(inifile_lookup $CONFIG_FILE Elasticsearch index)
type=$(inifile_lookup $CONFIG_FILE Elasticsearch type)


# Create the index and type
curl -X PUT -H 'Content-Type: application/json' "${host_and_port}/${index}" -d@schema.json
