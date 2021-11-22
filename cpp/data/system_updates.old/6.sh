#!/bin/bash
set -o errexit -o nounset

if [ -f /etc/lsb-release ]; then
    apt-get --quiet --yes install apparmor-utils
fi
