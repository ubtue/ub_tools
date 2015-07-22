#!/bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Creates the vufind user and setup apache to use this user.
#
set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

username="vufind"
# Check groups
if [[ $(cut -d: -f1 /etc/group | grep ^$username$) != "$username" ]] ; then
  sudo groupadd "$username"
fi
# Check user
if [[ $(cut -d: -f1 /etc/passwd | grep ^$username$) != "$username" ]] ; then
  sudo useradd --no-create-home -g "$username" --shell /bin/false "$username"
fi

# Apache should run with the new user.
if [[ -f "/etc/apache2/envvars" ]] ; then
  sudo sed --in-place=.bak --expression="s/export APACHE_RUN_USER=[a-zA-Z\-]*/export APACHE_RUN_USER=$username/g" \
                           --expression="s/export APACHE_RUN_GROUP=[a-zA-Z\-]*/export APACHE_RUN_GROUP=$username/g" \
                           "/etc/apache2/envvars"
elif [[ -f "/etc/httpd/conf/httpd.conf" ]] ; then
  sudo sed --in-place=.bak  --expression="s/User [a-zA-Z\-]*/User $username/g" \
                            --expression="s/Group [a-zA-Z\-]*/Group $username/g" \
                            "/etc/httpd/conf/httpd.conf"
else
  logger -s "${0##*/} - ERROR: Apache directory wasn't found!"
  exit 1
fi