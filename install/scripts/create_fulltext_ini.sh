#!/bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Clones a git repository to the current directory
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
Edits VuFind's fulltext.ini config file to enable Tika full-text indexing.
USAGE: ${0##*/} UB_TOOLS_CLONE_DIRECTORY

EOF
}


if [[ "$#" -ne 2 ]]; then
    echo "ERROR: Illegal number of parameters!"
    echo "Parameters: $*"
    show_help
    exit 1
fi
UB_TOOLS_CLONE_DIRECTORY=$1


function replace_line() {
    sed --in-place "/$1/c$2" $3
}


LOCAL_FULL_TEXT_INI=$VUFIND_LOCAL_DIR/config/vufind/fulltext.ini
cp $VUFIND_HOME/config/vufind/fulltext.ini $LOCAL_FULL_TEXT_INI
replace_line ';parser = Tika' 'parser = Tika' $LOCAL_FULL_TEXT_INI 
replace_line ';path = "/usr/local/tika/tika.jar"' 'path = "/usr/local/tika/tika.jar"' $LOCAL_FULL_TEXT_INI

cp $UB_TOOLS_CLONE_DIRECTORY/install/data/tika*.jar /usr/local/tika/tika.jar
