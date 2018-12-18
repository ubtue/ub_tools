#!/bin/bash
set -e


CONFIG_FILE=/usr/local/var/lib/tuelib/Elasticsearch.conf

host_and_port=$(cat inifile_lookup $CONFIG_FILE Elasticsearch host)
index=$(cat inifile_lookup $CONFIG_FILE Elasticsearch index)
type=$(cat inifile_lookup $CONFIG_FILE Elasticsearch type)


# Delete the index:
curl -X DELETE "${host_and_port}/${index}"
