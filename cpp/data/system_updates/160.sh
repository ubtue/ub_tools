#!/bin/bash
set -o errexit

if [ -r /etc/debian_version ]; then
    apt-get --yes install libboost1.74-tools-dev
fi
