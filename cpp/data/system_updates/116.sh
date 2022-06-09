#!/bin/bash
set -o errexit

if [ -r /etc/debian_version ]; then
    apt-get --yes install nlohmann-json3-dev
fi
