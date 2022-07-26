#!/bin/bash
set -e


#host_and_port=$(inifile_lookup /usr/local/var/lib/tuelib/Elasticsearch.conf Elasticsearch host)
host_and_port='https://localhost:9200'


# Delete the indices:
for schema in *_schema.json; do
    index="${schema%_schema.json}"
    curl -u 'elastic:5ygLJIa7nEpy_N67SFd+' -k --request DELETE --header 'Content-Type: application/json' "${host_and_port}/${index}"
    echo
done    
