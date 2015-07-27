#!/bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Creates a symbolic link from a vufind config to the apache config directory
#
set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

if [ -d  "/etc/apache2/conf-enabled/" ] ; then
	ln -sf "$VUFIND_LOCAL_DIR/httpd-vufind.conf"        "/etc/apache2/conf-enabled/vufind2.conf"
	ln -sf "$VUFIND_LOCAL_DIR/httpd-vufind-vhosts.conf" "/etc/apache2/conf-enabled/vufind2-vhosts.conf"
elif [ -d  "/etc/httpd/conf.d/" ] ; then
	ln -sf "$VUFIND_LOCAL_DIR/httpd-vufind.conf"        "/etc/httpd/conf.d/vufind2.conf"
	ln -sf "$VUFIND_LOCAL_DIR/httpd-vufind-vhosts.conf" "/etc/httpd/conf.d/vufind2-vhosts.conf"
else
	logger -s "${0##*/} - ERROR: Apache directory wasn't found!"
	exit 1;
fi
