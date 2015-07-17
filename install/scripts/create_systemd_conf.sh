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

if [ ! -d "$SYSTEMD_DIRECTORY" ] ; then
  sudo mkdir --parents "$SYSTEMD_DIRECTORY"
fi
sudo cp "$TPL" "$OUTPUT"

sudo systemctl enable httpd.service
sudo systemctl enable mariadb.service
sudo systemctl enable vufind.service
