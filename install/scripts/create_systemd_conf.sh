#!/bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Substitutes placeholders of templates/httpd-vufind.conf
# and copies the file to the right location.
#
set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
TEMPLATE_DIR=$SCRIPT_DIR/templates
TPL=$TEMPLATE_DIR/vufind.systemD
SYSTEMD_DIRECTORY="/usr/local/lib/systemd/system"
OUTPUT="$SYSTEMD_DIRECTORY/vufind.service"

# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

# Create service
if [ ! -d "$SYSTEMD_DIRECTORY" ] ; then
  mkdir --parents "$SYSTEMD_DIRECTORY"
fi
cp "$TPL" "$OUTPUT"

# Activate services
systemctl enable httpd.service
systemctl enable mariadb.service
systemctl enable vufind.service

# Start services (exept of VuFind)
systemctl start httpd.service
systemctl start mariadb.service