#!/bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Creates all directories and the vufind user.
# Sets all file privileges of VuFind and all other needed files.
#
set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

OWNER="vufind:vufind"

chown -R "$OWNER" "$VUFIND_HOME"
chmod +xr "$VUFIND_HOME"
chmod +xr "$VUFIND_LOCAL_DIR"
chown -R "$OWNER" "$VUFIND_LOCAL_DIR/cache"
# chown "$OWNER" "$VUFIND_LOCAL_DIR/config"
chown -R "$OWNER" "$VUFIND_LOCAL_DIR/logs/"
touch "$VUFIND_LOCAL_DIR/logs/record.xml"
touch "$VUFIND_LOCAL_DIR/logs/search.xml"
chown "$OWNER" "$VUFIND_HOME/local/logs/record.xml"
chown "$OWNER" "$VUFIND_HOME/local/logs/search.xml"

touch "/var/log/vufind.log"
touch "$VUFIND_LOCAL_DIR/import/solrmarc.log"
chown "$OWNER" "$VUFIND_LOCAL_DIR/import/solrmarc.log"
chown "$OWNER" "/var/log/vufind.log"
mkdir --parents "$VUFIND_LOCAL_DIR/config/vufind/local_overrides"
chown "$OWNER" "$VUFIND_LOCAL_DIR/config/vufind/local_overrides"
chmod +xr "$VUFIND_LOCAL_DIR/config/vufind/local_overrides"

if [[ -e "/usr/sbin/setsebool" ]]; then
  setsebool -P httpd_can_network_connect=1 \
                    httpd_can_network_connect_db=1 \
                    httpd_enable_cgi=1
fi

if [[ -e "/usr/bin/chcon" ]]; then
  chcon --recursive unconfined_u:object_r:httpd_sys_rw_content_t:s0 "$VUFIND_HOME"
  chcon system_u:object_r:httpd_config_t:s0 "$VUFIND_LOCAL_DIR/httpd-vufind.conf"
  chcon system_u:object_r:httpd_config_t:s0 "$VUFIND_LOCAL_DIR/httpd-vufind-vhosts.conf"
  chcon unconfined_u:object_r:httpd_sys_rw_content_t:s0 "$VUFIND_LOCAL_DIR/logs/record.xml"
  chcon unconfined_u:object_r:httpd_sys_rw_content_t:s0 "$VUFIND_LOCAL_DIR/logs/search.xml"
  chcon system_u:object_r:httpd_log_t:s0 /var/log/vufind.log
fi
