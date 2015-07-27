#!/bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Creates the vufind user and setup apache to use this user.
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
Creates the vufind user and setup apache to use this user.

USAGE: ${0##*/} USER_NAME USER_GROUP

USER_NAME    The user name of the vufind user
USER_GROUP   The user group of the vufind user
EOF
}

if [ "$#" -ne 2 ] ; then
  show_help
  exit 1
fi

USER_NAME=$1
USER_GROUP=$2
# Check groups
if [[ $(cut -d: -f1 /etc/group | grep ^$USER_GROUP$) != "$USER_GROUP" ]] ; then
  groupadd "$USER_GROUP"
fi
# Check user
if [[ $(cut -d: -f1 /etc/passwd | grep ^$USER_NAME$) != "$USER_NAME" ]] ; then
  useradd --no-create-home -g "$USER_GROUP" --shell /bin/false "$USER_NAME"
fi

# Apache should run with the new user.
if [[ -f "/etc/apache2/envvars" ]] ; then
  sed --in-place=.bak --expression="s/export APACHE_RUN_USER=[a-zA-Z0-9\-]*/export APACHE_RUN_USER=$USER_NAME/g" \
                           --expression="s/export APACHE_RUN_GROUP=[a-zA-Z0-9\-]*/export APACHE_RUN_GROUP=$USER_GROUP/g" \
                           "/etc/apache2/envvars"
elif [[ -f "/etc/httpd/conf/httpd.conf" ]] ; then
  sed --in-place=.bak  --expression="s/User [a-zA-Z0-9\-]*/User $USER_NAME/g" \
                            --expression="s/Group [a-zA-Z0-9\-]*/Group $USER_GROUP/g" \
                            "/etc/httpd/conf/httpd.conf"
else
  logger -s "${0##*/} - ERROR: Apache directory wasn't found!"
  exit 1
fi