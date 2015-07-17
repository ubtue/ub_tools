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
OUTPUT="/usr/local/lib/systemd/system/vufind.service"

sudo cp "$TPL" "$OUTPUT"

systemctl enable httpd.service
systemctl enable mariadb.service
systemctl enable vufind.service
