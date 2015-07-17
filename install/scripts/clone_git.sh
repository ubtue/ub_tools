#!/bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Clones a git repository to the current directory
#
set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

show_help() {
	cat << EOF
Clones a git repository to the current directory

USAGE: ${0##*/} GIT_REPOSITORY DIRECTORY

GIT_REPOSITORY      Url of the git repository
DIRECTORY           The name of the subdirectory for the cloned repository

EOF
}


if [ "$#" -ne 2 ]; then
  echo "ERROR: Illegal number of parameters!"
  echo "Parameters: $*"
  show_help
  exit 1
fi

if [[ "$1" != git@* ]]; then
	echo "${0##*/} - If you want to change the repository, use a git URL instead of httpds!"
fi

git clone "$1" "$2"
if [ $? -ne 0 ]; then
	logger -s "${0##*/} - ERROR. Couldn't clone git repository '$1' to '$2'"
	exit 1;
fi
