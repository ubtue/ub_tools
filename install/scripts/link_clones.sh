#!/bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
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
USAGE: ${0##*/} VUFIND_CLONE_PATH

VUFIND_CLONE_PATH     The path to the vufind clone
UB_TOOLS_CLONE_PATH   The path to the ub_tools clone
EOF
}

if [ "$#" -ne 2 ]; then
  echo "ERROR: Illegal number of parameters!"
  echo "Parameters: $*"
  show_help
  exit 1
fi

LOCAL_COPY_DIRECTORY="$1"
UB_TOOLS_CLONE_PATH="$2"


# This is a hack to get the link to ub_tools
rm --recursive --force "$VUFIND_HOME"
rm --recursive --force "$VUFIND_HOME/../ub_tools"
mkdir --parents "$VUFIND_HOME"
ln --force --symbolic --no-target-directory "$UB_TOOLS_CLONE_PATH" "$VUFIND_HOME/../ub_tools"

rm --recursive --force "$VUFIND_HOME"
ln --force --symbolic --no-target-directory  "$LOCAL_COPY_DIRECTORY" "$VUFIND_HOME"
