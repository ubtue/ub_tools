#!/bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Creates the vufind user and setup apache to use this user.
#
set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

username="vufind"
if [[ $(cut -d: -f1 /etc/passwd | grep ^vufind$) == 'vufind' ]] ; then
	echo "" > /dev/null # do nothing
else
	sudo groupadd "$username"
	sudo useradd --no-create-home --group "$username" --shell /bin/false "$username"
fi

# Apache should run with the new user.
if [[ -e "/etc/apache2/envvars" ]] ; then
	ENVVARS_PATH=/etc/apache2/envvars
    OUTPUT=$(cat $ENVVARS_PATH | sed --expression="s/export APACHE_RUN_USER=[a-zA-Z\-]*/export APACHE_RUN_USER=$username/g" \
                            --expression="s/export APACHE_RUN_GROUP=[a-zA-Z\-]*/export APACHE_RUN_GROUP=$username/g")
	sudo su -c "echo \"$OUTPUT\" > $ENVVARS_PATH"
elif [[ -e "/etc/httpd/conf/httpd.conf" ]] ; then
	CONFIG_PATH=/etc/apache2/envvars
	OUTPUT=$(cat $CONFIG_PATH | sed --expression="s/User [a-zA-Z\-]*/User $username/g" \
                           --expression="s/Group [a-zA-Z\-]*/Group $username/g")
	sudo su -c "echo \"$OUTPUT\" > $CONFIG_PATH"
else
	logger -s "${0##*/} - ERROR: Apache directory wasn't found!"
	exit 1
fi
