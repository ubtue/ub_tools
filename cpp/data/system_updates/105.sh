#!/bin/bash
set -o errexit


# Install clang-format-12 on Ubuntu systems:
if [ -r /etc/debian_version ]; then
    add-apt-repository -y ppa:alex-p/tesseract-ocr5
    apt install --yes tesseract-ocr
    apt install --yes tesseract-ocr-all
fi
