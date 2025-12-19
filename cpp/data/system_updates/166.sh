#!/bin/bash
set -o errexit

if ! dpkg -l | grep -q '^ii.*needrestart '; then
    apt-get update -qq
    apt-get --yes install needrestart
fi
systemctl is-enabled --quiet needrestart || systemctl enable --quiet needrestart
systemctl is-active --quiet needrestart || systemctl start --quiet needrestart
