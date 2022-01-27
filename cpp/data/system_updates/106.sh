#!/bin/bash
set -o errexit
<<<<<<< HEAD
if [[ ! -e /usr/local/ub_tools/bsz_daten/beacon_downloads ]]; then
    mkdir /usr/local/ub_tools/bsz_daten/beacon_downloads
fi
if [[ -f /usr/local/ub_tools/bsz_daten/kalliope.staatsbibliothek-berlin.beacon ]]; then
    mv /usr/local/ub_tools/bsz_daten/kalliope.staatsbibliothek-berlin.beacon /usr/local/ub_tools/bsz_daten/beacon_downloads
fi
if [[ -f /usr/local/ub_tools/bsz_daten/archivportal-d.beacon ]]; then
    mv /usr/local/ub_tools/bsz_daten/archivportal-d.beacon /usr/local/ub_tools/bsz_daten/beacon_downloads
=======


# Install clang-format-12 on Ubuntu systems:
if [ -r /etc/debian_version ]; then
    add-apt-repository -y ppa:alex-p/tesseract-ocr5
    apt install --yes tesseract-ocr
    apt install --yes tesseract-ocr-all
>>>>>>> 841e8f093186639d3733a9929bec4589901b21ae
fi
