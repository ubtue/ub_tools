#! /bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Writes vufind path settings to /etc/profile.d/vufind.sh
#

# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

echo "export JAVA_HOME=$JAVA_HOME"                > /etc/profile.d/vufind.sh
echo "export VUFIND_HOME=$VUFIND_HOME"           >> /etc/profile.d/vufind.sh
echo "export VUFIND_LOCAL_DIR=$VUFIND_LOCAL_DIR" >> /etc/profile.d/vufind.sh
chmod 644 "/etc/profile.d/vufind.sh"
source "/etc/profile.d/vufind.sh"
