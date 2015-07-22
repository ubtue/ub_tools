#!/bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Creates all directories and the vufind user.
# Sets all file privileges of VuFind and all other needed files.
#
set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

OWNER="vufind:vufind"

sudo chown -R "$OWNER" "$VUFIND_HOME"
sudo chmod +xr "$VUFIND_HOME"
sudo chmod +xr "$VUFIND_LOCAL_DIR"
sudo chown -R "$OWNER" "$VUFIND_LOCAL_DIR/cache"
# sudo chown "$OWNER" "$VUFIND_LOCAL_DIR/config"
sudo chown -R "$OWNER" "$VUFIND_LOCAL_DIR/logs/"
sudo touch "$VUFIND_LOCAL_DIR/logs/record.xml"
sudo touch "$VUFIND_LOCAL_DIR/logs/search.xml"
sudo chown "$OWNER" "$VUFIND_HOME/local/logs/record.xml"
sudo chown "$OWNER" "$VUFIND_HOME/local/logs/search.xml"

sudo touch "/var/log/vufind.log"
sudo touch "$VUFIND_LOCAL_DIR/import/solrmarc.log"
sudo chown "$OWNER" "$VUFIND_LOCAL_DIR/import/solrmarc.log"
sudo chown "$OWNER" "/var/log/vufind.log"
sudo mkdir --parents "$VUFIND_LOCAL_DIR/config/vufind/local_overrides"
sudo chown "$OWNER" "$VUFIND_LOCAL_DIR/config/vufind/local_overrides"
sudo chmod +xr "$VUFIND_LOCAL_DIR/config/vufind/local_overrides"

if [[ -e "/usr/sbin/setsebool" ]]; then
  sudo setsebool httpd_can_network_connect 1
fi

if [[ -e "/usr/bin/chcon" ]]; then
  sudo chcon --recursive unconfined_u:object_r:httpd_sys_rw_content_t:s0 "$VUFIND_HOME"
  sudo chcon system_u:object_r:httpd_config_t:s0 "$VUFIND_LOCAL_DIR/httpd-vufind.conf"
  sudo chcon system_u:object_r:httpd_config_t:s0 "$VUFIND_LOCAL_DIR/httpd-vufind-vhosts.conf"
  sudo chcon unconfined_u:object_r:httpd_sys_rw_content_t:s0 "$VUFIND_LOCAL_DIR/logs/record.xml"
  sudo chcon unconfined_u:object_r:httpd_sys_rw_content_t:s0 "$VUFIND_LOCAL_DIR/logs/search.xml"
  sudo chcon system_u:object_r:httpd_log_t:s0 /var/log/vufind.log
fi
