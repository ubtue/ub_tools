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
TPL=$TEMPLATE_DIR/vufind.upstart
OUTPUT="/etc/init.d/vufind"

sudo cp "$TPL" "$OUTPUT"
sudo chmod 755 "$OUTPUT"

sudo update-rc.d mysql defaults 
sudo update-rc.d apache2 defaults 
sudo update-rc.d vufind defaults 