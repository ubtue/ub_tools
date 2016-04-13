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
touch "$VUFIND_LOCAL_DIR/logs/record.xml"
touch "$VUFIND_LOCAL_DIR/logs/search.xml"

mkdir --parents "$VUFIND_LOCAL_DIR/import"
touch "$VUFIND_LOCAL_DIR/import/solrmarc.log"
mkdir --parents "$VUFIND_LOCAL_DIR/config/vufind/local_overrides"
chmod +xr "$VUFIND_LOCAL_DIR/config/vufind/local_overrides"

chown -R "$OWNER" "$VUFIND_HOME"
chown -R "$OWNER" "$CLONE_DIRECTORY"

touch "/var/log/vufind.log"
chown "$OWNER" "/var/log/vufind.log"

mkdir --parents "/tmp/vufind_sessions/"
chown -R "$OWNER" "/tmp/vufind_sessions/"

mkdir --parents "/var/lib/tuelib/bibleRef"
chown -R "$OWNER" "/var/lib/tuelib"

mkdir --parents "/var/log/$SYSTEM_TYPE"
chown -R "$OWNER" "/var/log/$SYSTEM_TYPE"

function set_se_perms() {
  for arg_no in $(seq 2 $#); do
    arg="${!arg_no}"
    if [[ -d "$arg" ]]; then # Recursively process all directories.
       find "$arg" -type d -print0 | xargs --null --max-lines semanage fcontext --add --type "$1"
       find "$arg" -type d -print0 | xargs --null --max-lines restorecon -R 
    else # Assume we have an ordinary file.
      semanage fcontext --add --type "$1" "$arg"
      restorecon -R "$arg"
    fi
  done
}

if [[ $(which getenforce) && $(getenforce) == "Enforcing" ]] ; then

  if [[ $(which setsebool) ]]; then
    setsebool -P httpd_can_network_connect=1 httpd_can_network_connect_db=1 httpd_enable_cgi=1
  fi

  if [[ $(which semanage) ]]; then
    set_se_perms httpd_config_t /var/lib/tuelib
    set_se_perms httpd_sys_rw_content_t "$VUFIND_HOME"
    set_se_perms httpd_config_t "$VUFIND_HOME"/local/httpd-vufind*.conf
    set_se_perms httpd_log_t /var/log/vufind.log
    set_se_perms var_log_t /var/log/"$SYSTEM_TYPE"
    set_se_perms system_u:object_r:bin_t:s0 /usr/local/bin
  fi

else
  echo "##########################################################################################"
  echo "# WARNING: SELinux is either not properly installed or currently disabled on this system #" 
  echo "# Skipped SELinux configuration...							 #"
  echo "##########################################################################################"
fi

