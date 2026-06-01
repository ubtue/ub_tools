#!/bin/bash

FILE="/usr/local/var/lib/tuelib/Elasticsearch.conf"

if [ ! -f "$FILE" ]; then
    echo "Error: Configuration file $FILE not found."
    exit 1
fi


HOST_AND_PORT=$(inifile_lookup "$FILE" Elasticsearch host)

if [ $? -ne 0 ]; then
    echo "Error: Could not read host from configuration file."
    exit 1
fi

authorization_args=()
username_login_args=()

URL_ADD_PIPELINE="$HOST_AND_PORT/_ingest/pipeline/add_timestamp"
URL_SET_DEFAULT_PIPELINE_FULLTEXT_CACHE="$HOST_AND_PORT/full_text_cache/_settings"
URL_SET_DEFAULT_PIPELINE_FULLTEXT_CACHE_HTML="$HOST_AND_PORT/full_text_cache_html/_settings"
URL_SET_DEFAULT_PIPELINE_FULLTEXT_CACHE_URLS="$HOST_AND_PORT/full_text_cache_urls/_settings"

TOKEN=$(inifile_lookup "$FILE" Elasticsearch token "")

if [ -n "$TOKEN" ]; then
  authorization_args=(-H "Authorization: ApiKey $TOKEN")
else
  username=$(inifile_lookup "$FILE" Elasticsearch username "")
  password=$(inifile_lookup "$FILE" Elasticsearch password "")

  if [ -n "$username" ] && [ -n "$password" ]; then
    username_login_args=(-u "$username:$password")
  fi
fi


curl -X PUT "$URL_ADD_PIPELINE" \
    "${username_login_args[@]}" \
    "${authorization_args[@]}" \
    -H "Content-Type: application/json" \
    -d '{
      "description": "Add last_update timestamp",
      "processors": [
        {
          "set": {
            "field": "last_update",
            "value": "{{_ingest.timestamp}}"
          }
        }
      ]
    }'

curl -X PUT "$URL_SET_DEFAULT_PIPELINE_FULLTEXT_CACHE" \
    "${username_login_args[@]}" \
    "${authorization_args[@]}" \
    -H "Content-Type: application/json" \
    -d '{
        "index.default_pipeline": "add_timestamp"
      }'

curl -X PUT "$URL_SET_DEFAULT_PIPELINE_FULLTEXT_CACHE_HTML" \
    "${username_login_args[@]}" \
    "${authorization_args[@]}" \
    -H "Content-Type: application/json" \
    -d '{
        "index.default_pipeline": "add_timestamp"
      }'

curl -X PUT "$URL_SET_DEFAULT_PIPELINE_FULLTEXT_CACHE_URLS" \
    "${username_login_args[@]}" \
    "${authorization_args[@]}" \
    -H "Content-Type: application/json" \
    -d '{
        "index.default_pipeline": "add_timestamp"
      }'