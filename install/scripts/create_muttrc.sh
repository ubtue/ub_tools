#!/bin/bash
#
# Set realname in muttrc to 

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

USAGE: ${0##*/} MUTT_REAL_NAME

MUTT_REAL_NAME real name that is written to roots muttrc to have a proper sender real name for the C++ programs of the pipeline

EOF
}

if [ "$#" -ne 1 ]; then
  echo "ERROR: Illegal number of parameters!"
  echo "Parameters: $*"
  show_help
  exit 1
fi

MUTT_REALNAME=$1
MUTTRC_FILE=~/.muttrc

# Skip if a real name is already set
if fgrep --quiet --recursive --ignore-case 'set realname' $MUTTRC_FILE; then
  exit 0
else
  echo "set realname='$MUTT_REALNAME'" >> $MUTTRC_FILE
fi











