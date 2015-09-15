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
TPL=$TEMPLATE_DIR/logrotate.conf
OUTPUT=/etc/logrotate.d/vufind

# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

cp "$TPL" "$OUTPUT"
