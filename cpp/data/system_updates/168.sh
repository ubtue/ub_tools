#!/bin/bash

FILE="/usr/local/var/lib/tuelib/Elasticsearch.conf"

if [ ! -f "$FILE" ]; then
    echo "Error: Configuration file $FILE not found."
    # returning exit 0 to avoid breaking the update process if the configuration file is missing
    exit 0
fi


HOST_AND_PORT=$(inifile_lookup "$FILE" Elasticsearch host)

if [ $? -ne 0 ]; then
    echo "Error: Could not read host from configuration file."
    # returning exit 0 to avoid breaking the update process if the host cannot be read from the configuration file
    exit 0
fi


SCRIPT_FILE="/usr/local/ub_tools/cpp/elasticsearch/create_pipeline.sh"

# Check if the script file exists then execute it, otherwise print an error message and exit with a non-zero status.
if [ -f "$SCRIPT_FILE" ]; then
    bash "$SCRIPT_FILE"
else
    echo "Error: Script file $SCRIPT_FILE not found."
    exit 1
fi
