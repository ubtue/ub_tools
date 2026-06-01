#!/bin/bash


SCRIPT_FILE="/usr/local/ub_tools/cpp/elasticsearch/create_pipeline.sh"

# Check if the script file exists then execute it, otherwise print an error message and exit with a non-zero status.
if [ -f "$SCRIPT_FILE" ]; then
    bash "$SCRIPT_FILE"
else
    echo "Error: Script file $SCRIPT_FILE not found."
    exit 1
fi
