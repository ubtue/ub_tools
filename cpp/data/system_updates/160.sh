#!/bin/bash
set -o errexit

if [ -r /etc/debian_version ]; then
    apt-get --yes install libboost-all-dev
fi
