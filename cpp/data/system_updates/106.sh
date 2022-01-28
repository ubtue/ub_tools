#!/bin/bash
set -o errexit
if [[ ! -e /usr/local/ub_tools/bsz_daten/beacon_downloads ]]; then
    mkdir /usr/local/ub_tools/bsz_daten/beacon_downloads
fi
if [[ -f /usr/local/ub_tools/bsz_daten/kalliope.staatsbibliothek-berlin.beacon ]]; then
    mv /usr/local/ub_tools/bsz_daten/kalliope.staatsbibliothek-berlin.beacon /usr/local/ub_tools/bsz_daten/beacon_downloads/kalliope.staatsbibliothek-berlin.lr.beacon
fi
if [[ -f /usr/local/ub_tools/bsz_daten/archivportal-d.beacon ]]; then
    mv /usr/local/ub_tools/bsz_daten/archivportal-d.beacon /usr/local/ub_tools/bsz_daten/beacon_downloads/archivportal-d.lr.beacon
