#! /bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Writes vufind path settings to /etc/profile.d/vufind.sh
#

sudo sh -c "echo export JAVA_HOME=/usr/lib/jvm/default-java    > /etc/profile.d/vufind.sh"
sudo sh -c "echo export VUFIND_HOME=$VUFIND_HOME              >> /etc/profile.d/vufind.sh"
sudo sh -c "echo export VUFIND_LOCAL_DIR=$VUFIND_LOCAL_DIR    >> /etc/profile.d/vufind.sh"
sudo chmod 644 "/etc/profile.d/vufind.sh"
sudo sh -c "source /etc/profile.d/vufind.sh"
