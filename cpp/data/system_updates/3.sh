#!/bin/bash

if [ -e /etc/debian_version ]; then
    apt-get --assume-yes install fetchmail
else
    dnf --assumeyes install fetchmail
fi
