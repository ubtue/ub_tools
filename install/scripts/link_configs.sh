#! /bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Links some configs.
#
set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

show_help() {
    cat << EOF
Links some configs.

USAGE: ${0##*/} CONFIGS_DIRECTORY SYSTEM_TYPE

CONFIGS_DIRECTORY    The directory, which holds the configs.
SYSTEM_TYPE          Should be either krimdok or ixtheo

EOF
}

if [ "$#" -ne 2 ]; then
  echo "ERROR: Illegal number of parameters!"
  echo "Parameters: $*"
  show_help
  exit 1
fi

CONFIGS_DIRECTORY=$1
SYSTEM_TYPE=$2
ln --symbolic --force --no-target-directory "$CONFIGS_DIRECTORY/cronjobs" "/var/lib/tuelib/cronjobs"
if [ $SYSTEM_TYPE == "ixtheo" ]; then
  ln --symbolic --force --no-target-directory "$CONFIGS_DIRECTORY/translations.conf" "/var/lib/tuelib/translations.conf"
fi
ln --symbolic --force --no-target-directory "$CONFIGS_DIRECTORY/logrotate.conf" "/var/lib/tuelib/logrotate.conf"
