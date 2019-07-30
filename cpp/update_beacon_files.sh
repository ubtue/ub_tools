#!/bin/bash
# Runs through the phases of the IxTheo MARC processing pipeline.
set -o errexit -o nounset

cd /usr/local/ub_tools/bsz_daten
wget http://kalliope.staatsbibliothek-berlin.de/beacon/beacon.txt -O kalliope.staatsbibliothek-berlin.beacon
wget https://labs.ddb.de/app/beagen/item/213 -O archivportal-d.beacon
