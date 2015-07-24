#!/bin/bash
#
# 
#
set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

if [[ ! -x "/bin/python2" ]] && [[ -x "/usr/bin/python2" ]]; then
	ln -s "/usr/bin/python2" "/bin/python2"
fi