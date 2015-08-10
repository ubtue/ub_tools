#!/bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Creates a .htaccess with a .htpasswd configuration to protect the access
# to the website.
#
set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
TEMPLATE_DIR=$SCRIPT_DIR/templates
TPL=$TEMPLATE_DIR/htaccess
OUTPUT=$VUFIND_HOME/public/.htaccess

# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

show_help() {
  cat << EOF
Creates a .htaccess with a .htpasswd configuration to protect the access to the website.

USAGE: ${0##*/} NAME HTPASSWD

NAME
HTPASSWD     The path to a valid .htpasswd

EOF
}

if [ "$#" -ne 2 ]; then
  echo "ERROR: Illegal number of parameters!"
  echo "Parameters: $*"
  show_help
  exit 1
fi

NAME="$1"
HTPASSWD="$2"

TPL_CONTENT="$(cat $TPL)"

TMP=$(echo "$TPL_CONTENT" | sed -e "s|{{{VUFIND_HOME}}}|$VUFIND_HOME|g" \
                                -e "s|{{{VUFIND_LOCAL_DIR}}}|$VUFIND_LOCAL_DIR|g" \
                                -e "s|{{{NAME}}}|$NAME|g" \
                                -e "s|{{{HTPASSWD}}}|$HTPASSWD|g")

echo "$TMP" > "$OUTPUT"
