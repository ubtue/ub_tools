  #! /bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Writes the start script for a specific VuFind clone.
#
set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
TEMPLATE_DIR=$SCRIPT_DIR/templates
TPL=$TEMPLATE_DIR/start-vufind.sh


show_help() {
  cat << EOF
Writes the start script for a specific VuFind clone.

USAGE: ${0##*/} CLONE_DIRECTORY_PATH

CLONE_DIRECTORY_PATH     The path to the directory of the
                         specific VuFind clone.
EOF
}


if [ "$#" -ne 1 ]; then
  echo "ERROR: Illegal number of parameters!"
  echo "Parameters: $*"
  show_help
  exit 1
fi

CLONE_DIRECTORY_PATH="$1"
TPL_CONTENT="$(cat $TPL)"
OUTPUT="$CLONE_DIRECTORY_PATH/start-vufind.sh"

if [ -d "/etc/apache2" ] ; then
	WEBSERVER_NAME="apache2"
else
	WEBSERVER_NAME="httpd"
fi

TMP=$(echo "$TPL_CONTENT" | sed -e "s|{{{VUFIND_HOME}}}|$VUFIND_HOME|g" \
                                -e "s|{{{VUFIND_LOCAL_DIR}}}|$VUFIND_LOCAL_DIR|g" \
                                -e "s|{{{CLONE_DIRECTORY_PATH}}}|$CLONE_DIRECTORY_PATH|g" \
                                -e "s|{{{WEBSERVER_NAME}}}|$WEBSERVER_NAME|g")

echo "$TMP" > "$OUTPUT"
sudo chmod ug+x "$OUTPUT"
