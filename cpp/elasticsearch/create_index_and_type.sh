#!/bin/bash
set -e


CONFIG_FILE=/usr/local/var/lib/tuelib/Elasticsearch.conf

host_and_port=$(cat inifile_lookup $CONFIG_FILE Elasticsearch host)
index=$(cat inifile_lookup $CONFIG_FILE Elasticsearch index)
type=$(cat inifile_lookup $CONFIG_FILE Elasticsearch type)


# Create the index:
curl -X PUT "${host_and_port}/${index}"

# Create the type:
curl -X POST "${host_and_port}/${index}/_mappings/${type}" -d@schema.json
