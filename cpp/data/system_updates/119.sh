#!/bin/bash
set -o errexit

# java version has been manually set to 8 in the installer,
# revert this to "auto" mode and use 11 instead
apt-get --yes install openjdk-11-jdk
update-alternatives --set java /usr/lib/jvm/java-11-openjdk-amd64/bin/java
update-alternatives --set javac /usr/lib/jvm/java-11-openjdk-amd64/bin/javac
if [[ -e /usr/local/vufind ]]; then
    OLD_DIR=$PWD
    cd /usr/local/ub_tools/java && make install
    cd "$OLD_DIR"
    systemctl restart vufind
fi
