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

function show_help() {
  cat << EOF
Creates all directories and the vufind user. Sets all file privileges of VuFind and all other needed files.

USAGE: ${0##*/} CLONE_DIRECTORY USER_NAME USER_GROUP SYSTEM_TYPE

CLONE_DIRECTORY  The name of the subdirectory for the cloned repository
USER_NAME        The user name of the vufind user
USER_GROUP       The user group of the vufind user
SYSTEM_TYPE      Should be either "krimdok" or "ixtheo".
EOF
}

if [ "$#" -ne 4 ] ; then
  show_help
  exit 1
fi

CLONE_DIRECTORY=$1
USER_NAME=$2
USER_GROUP=$3
SYSTEM_TYPE=$4
OWNER="$USER_NAME:$USER_GROUP"

chmod +xr "$VUFIND_HOME"
chmod +xr "$VUFIND_LOCAL_DIR"
[ ! -d "$VUFIND_LOCAL_DIR/logs" ] && mkdir "$VUFIND_LOCAL_DIR/logs"
touch "$VUFIND_LOCAL_DIR/logs/record.xml"
touch "$VUFIND_LOCAL_DIR/logs/search.xml"

[ ! -d "$VUFIND_LOCAL_DIR/import" ] && mkdir --parents "$VUFIND_LOCAL_DIR/import"
touch "$VUFIND_LOCAL_DIR/import/solrmarc.log"
[ ! -d "$VUFIND_LOCAL_DIR/config/vufind/local_overrides" ] && mkdir --parents "$VUFIND_LOCAL_DIR/config/vufind/local_overrides"
chmod +xr "$VUFIND_LOCAL_DIR/config/vufind/local_overrides"

chown -R "$OWNER" "$VUFIND_HOME"
chown -R "$OWNER" "$CLONE_DIRECTORY"

touch "/var/log/vufind.log"
chown "$OWNER" "/var/log/vufind.log"

[ ! -d "/tmp/vufind_sessions/" ] && mkdir --parents "/tmp/vufind_sessions/"
chown -R "$OWNER" "/tmp/vufind_sessions/"

[ ! -d "/var/lib/tuelib/bibleRef" ] && mkdir --parents "/var/lib/tuelib/bibleRef"
chown -R "$OWNER" "/var/lib/tuelib"

[ ! -d "/var/log/$SYSTEM_TYPE" ] && mkdir --parents "/var/log/$SYSTEM_TYPE"
chown -R "$OWNER" "/var/log/$SYSTEM_TYPE"

[ ! -d "/var/www/cgi-bin" ] && mkdir --parents "/var/www/cgi-bin"
# chown isn't necessary here

function set_se_permissions() {
  semanage fcontext --add --type "$1" "$2"
  if [ $# -ne 2 ]; then
    restorecon -R "$3"
  else
    restorecon -R "$2"
  fi
}

if [[ $(which getenforce) && $(getenforce) == "Enforcing" ]] ; then

   if [[ $(which setsebool) ]]; then
    setsebool -P httpd_can_network_connect=1 httpd_can_network_connect_db=1 httpd_enable_cgi=1 httpd_can_sendmail=1
  fi

  if [[ $(which semanage) ]]; then
    set_se_permissions public_content_t "/var/lib/tuelib"
    set_se_permissions httpd_sys_rw_content_t "/usr/local/vufind2(/.*)?" "/usr/local/vufind2"
    set_se_permissions httpd_config_t "$VUFIND_HOME/local/httpd-vufind*.conf"
    set_se_permissions httpd_log_t "/var/log/vufind.log"
    set_se_permissions var_log_t "/var/log/$SYSTEM_TYPE"
    set_se_permissions bin_t "/usr/local/bin"
    set_se_permissions bin_t "/var/www/cgi-bin"
    set_se_permissions httpd_sys_content_t "/usr/local/ub_tools/configs/ixtheo/translations.conf"
    
    if [ -f "/var/lib/tuelib/full_text.db" ]; then
      set_se_permissions public_content_t "/var/lib/tuelib/full_text.db"
    fi
  fi

else
  echo "##########################################################################################"
  echo "# WARNING: SELinux is either not properly installed or currently disabled on this system #" 
  echo "# Skipped SELinux configuration...							 #"
  echo "##########################################################################################"
fi

