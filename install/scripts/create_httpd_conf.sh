#!/bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Substitutes placeholders of templates/httpd-vufind.conf
# and copies the file to the right location.
#
set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
TEMPLATE_DIR=$SCRIPT_DIR/templates
TPL=$TEMPLATE_DIR/httpd-vufind.conf
OUTPUT=$VUFIND_LOCAL_DIR/httpd-vufind.conf

show_help() {
  cat << EOF
Substitutes placeholders of templates/httpd-vufind.conf and copies the file to the right location.

USAGE: ${0##*/} MODULES FORCE_SSL

MODULES      A comma seperated list of custom vufind module names
FORCE_SSL    A boolean value (true/false) to indicate to redirect
             http to https

EOF
}

if [ "$#" -ne 2 ]; then
  echo "ERROR: Illegal number of parameters!"
  echo "Parameters: $*"
  show_help
  exit 1
fi

MODULES=$1
FORCE_SSL=$2

TPL_CONTENT="$(cat $TPL)"
PLACE=$(echo "$TPL_CONTENT" | awk 'match($0, /{{{if forcessl [A-Za-z0-9\.\-]*}}}/) { print $0 }')

if [[ "$PLACE" ]] && [[ "$FORCE_SSL" = true ]]; then
	INC_TPL=$TEMPLATE_DIR/$(echo $PLACE | sed 's/{{{if ssl \([A-Za-z0-9\.\-]*\)}}}/\1/g')
	INC_TPL_CONTENT="$(cat $INC_TPL)"
	TPL_CONTENT=$(echo "$TPL_CONTENT" | awk -v r="$INC_TPL_CONTENT" "{gsub(/$PLACE/,r)}1")
elif [[ "$PLACE" ]] ; then
	TPL_CONTENT=$(echo "$TPL_CONTENT" | sed "s|$PLACE||g")
fi

TMP=$(echo "$TPL_CONTENT" | sed -e "s|{{{VUFIND_HOME}}}|$VUFIND_HOME|g" \
                                -e "s|{{{VUFIND_LOCAL_DIR}}}|$VUFIND_LOCAL_DIR|g" \
                                -e "s|{{{modules}}}|$MODULES|g")

echo "$TMP" > $OUTPUT