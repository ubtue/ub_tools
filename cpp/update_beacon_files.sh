#!/bin/bash
# Runs through the phases of the IxTheo MARC processing pipeline.
set -o errexit -o nounset

cd /usr/local/ub_tools/bsz_daten
wget https://labs.ddb.de/app/beagen/item/person/archive/latest -O archivportal-d.beacon
wget http://kalliope.staatsbibliothek-berlin.de/beacon/beacon.txt -O kalliope.staatsbibliothek-berlin.beacon

sed -i -e 's/#FORMAT: GND-BEACON/#FORMAT: BEACON/g' kalliope.staatsbibliothek-berlin.beacon
