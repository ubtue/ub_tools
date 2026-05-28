#!/bin/bash
set -o errexit

# The authorization header is need when running v9 or newer, but not when running v8 with basic auth. The API key can be generated in the Elasticsearch management interface. It should have the "manage_pipeline" role, which is sufficient for creating ingest pipelines.
curl -X PUT "http://localhost:9200/_ingest/pipeline/add_timestamp" \
  # -H "Authorization: ApiKey YOUR_TOKEN" \
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