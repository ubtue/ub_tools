#!/bin/bash
set -o errexit -o nounset

if [ -f /etc/lsb-release ]; then
    apt-get --quiet --yes install libpq-dev postgresql postgresql-client
else
    dnf --assumeyes install libpq-devel postgresql
