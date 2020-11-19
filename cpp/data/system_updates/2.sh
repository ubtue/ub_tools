#!/bin/bash
if [ -f /etc/redhat-release ]; then
    dnf config-manager --add-repo http://rpms.remirepo.net/enterprise/8/remi/x86_64/
    rpm --import https://build.opensuse.org/projects/home:Alexander_Pozdnyakov/public_key
    rpm --import https://rpms.remirepo.net/RPM-GPG-KEY-remi2018
    dnf --assumeyes install python3-pexpect yaz
else # Hopefully we're on Ubuntu!
    apt-get --quiet --yes --allow-unauthenticated install python3-pexpect yaz
fi
