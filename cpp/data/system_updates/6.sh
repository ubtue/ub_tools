#!/bin/bash
if [ -f /etc/lsb-release ]; then
    apt-get --quiet --yes install apparmor-utils
fi
