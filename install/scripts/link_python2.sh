#!/bin/bash
#
# 
#
set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )


if [[ ! -x "/bin/python2" ]] && [[ -x "/usr/bin/python2" ]]; then
	sudo ln -s "/usr/bin/python2" "/bin/python2"
fi