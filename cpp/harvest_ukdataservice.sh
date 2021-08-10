#!/bin/bash
#
# This is a script to harvest criminology statistics metadata from the UK Data Service.
#

readonly METADATA_PREFIX=oai_dc
readonly HARVEST_SET_OR_IDENTIFIER=identifier=8812
readonly CONTROL_NUMBER_PREFIX=UKDATASERVICE
readonly OUTPUT_FILENAME=ukdataservice.xml
readonly TIME_LIMIT_PER_REQUEST=60 # seconds
readonly DUPS_DATABASE=/usr/local/var/lib/tuelib/ukdataservice-dups.db
oai_pmh_harvester --path-to-dups-database="${DUPS_DATABASE}" \
                  'https://oai.ukdataservice.ac.uk:8443/oai/provider' \
                  "${METADATA_PREFIX}" "${HARVEST_SET_OR_IDENTIFIER}" "${CONTROL_NUMBER_PREFIX}" \
                  "${OUTPUT_FILENAME}" "${TIME_LIMIT_PER_REQUEST}"
