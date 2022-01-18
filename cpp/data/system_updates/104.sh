#!/bin/bash
set -o errexit


# Install clang-format-12 on Ubuntu systems:
if [ -r /etc/debian_version ]; then
    apt-get --quiet --yes --allow-unauthenticated install clang-format-12
fi
