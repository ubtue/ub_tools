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

# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

cp "$TPL" "$OUTPUT"
chmod 755 "$OUTPUT"

update-rc.d mysql defaults 
update-rc.d apache2 defaults 
update-rc.d vufind defaults

# Start services (exept of VuFind)
service mysql start
service apache2 start