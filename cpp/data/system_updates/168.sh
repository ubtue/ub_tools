#!/bin/bash

FILE="/usr/local/var/lib/tuelib/Elasticsearch.conf"

if [ ! -f "$FILE" ]; then
    exit 1
fi


authorization_args=()
username_login_args=()

URL_ADD_PIPELINE="http://localhost:9200/_ingest/pipeline/add_timestamp"
URL_SET_DEFAULT_PIPELINE_FULLTEXT_CACHE="http://localhost:9200/full_text_cache/_settings"
URL_SET_DEFAULT_PIPELINE_FULLTEXT_CACHE_HTML="http://localhost:9200/full_text_cache_html/_settings"
URL_SET_DEFAULT_PIPELINE_FULLTEXT_CACHE_URLS="http://localhost:9200/full_text_cache_urls/_settings"

TOKEN=$(inifile_lookup "$FILE" Elasticsearch token)

if [ $? -eq 0 ]; then
  if [ -n "$TOKEN" ]; then
    authorization_args=(-H "Authorization: ApiKey $TOKEN")
  fi
else
  username=$(inifile_lookup "$FILE" Elasticsearch username)
  password=$(inifile_lookup "$FILE" Elasticsearch password)

  if [ $? -eq 0 ]; then
    if [ -n "$username" ] && [ -n "$password" ]; then
      username_login_args=(-u "$username:$password")
    fi
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
