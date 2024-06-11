#!/bin/bash
set -o errexit

# updating jdk from 11 to 17
apt-get --quiet --yes install openjdk-17-jdk
update-java-alternatives --set java-1.17.0-openjdk-amd64

if [[ $TUEFIND_FLAVOUR == "krimdok" ]] || [[ $TUEFIND_FLAVOUR == "ixtheo" ]] ; then
    cd /usr/local/ub_tools/java/
    make install
    systemctl restart vufind
fi

