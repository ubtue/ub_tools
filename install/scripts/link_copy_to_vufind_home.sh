#!/bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Substitutes placeholders of templates/httpd-vufind.conf
# and copies the file to the right location.
#
set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )


show_help() {
  cat << EOF
Substitutes placeholders of templates/httpd-vufind.conf and copies the file to the right location.

USAGE: ${0##*/} CLONE_PATH

CLONE_PATH     The path to the vufind clone

EOF
}

if [ "$#" -ne 1 ]; then
  echo "ERROR: Illegal number of parameters!"
  echo "Parameters: $*"
  show_help
  exit 1
fi

LOCAL_COPY_DIRECTORY=$1

sudo ln -sfT $LOCAL_COPY_DIRECTORY $VUFIND_HOME