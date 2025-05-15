#!/bin/bash
# Send request to local zts docker container and get the logging output 
# since the request started

ZTS_SERVER_AND_PORT="127.0.0.1:1969"


if [ $# -ne 1 ]; then
    echo "Usage: $0 url"
    exit 1
fi

url="$1"

last_request=$(date --rfc-3339=seconds | sed 's/ /T/') && curl --fail -d "${url}" -H 'Content-Type: text/plain' http://"${ZTS_SERVER_AND_PORT}"/web && docker logs --since "$last_request" $(docker ps --filter ancestor=zts --last 1 --format "{{.ID}}")

