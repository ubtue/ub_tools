#!/bin/bash
set -o errexit -o nounset

if [ -f /etc/lsb-release ]; then
    apt-get --quiet --yes install libsystemd-dev
else
    dnf --assumeyes install systemd-devel
fi
