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

# Create service
if [ ! -d "$SYSTEMD_DIRECTORY" ] ; then
  sudo mkdir --parents "$SYSTEMD_DIRECTORY"
fi
sudo cp "$TPL" "$OUTPUT"

# Activate services
sudo systemctl enable httpd.service
sudo systemctl enable mariadb.service
sudo systemctl enable vufind.service

# SystemD needs a --no-background mode to run. So we have to override the original vufind.sh
VUFIND_SH_TPL="$TEMPLATE_DIR/vufind.sh"
VUFIND_SH="$VUFIND_HOME/vufind.sh"
sudo cp "$VUFIND_SH_TPL" "$VUFIND_SH"
