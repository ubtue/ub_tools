#!/bin/bash
#
#
set -o errexit -o nounset
BINARY_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SCRIPT_DIR=$BINARY_DIR/scripts

# @see: http://www.linuxjournal.com/content/normalizing-path-names-bash
function normalize_path()
{
    # Remove all /./ sequences.
    local   path=${1//\/.\//\/}
    
    # Remove first dir/.. sequence.
    local   npath=$(echo $path | sed -e 's;[^/][^/]*/\.\./;;')
    
    # Remove remaining dir/.. sequence.
    while [[ $npath != $path ]]
    do
        path=$npath
        npath=$(echo $path | sed -e 's;[^/][^/]*/\.\./;;')
    done
    echo $path
}




if [ "$#" -ne 1 ]; then
  echo "Path to config file is missing!"
  exit 1
else
	CONFIG_FILE="$1"
fi

export VUFIND_HOME="/usr/local/vufind2"
export VUFIND_LOCAL_DIR="$VUFIND_HOME/local"

NAME=""
VUFIND_CLONE_DIRECTORY=""
VUFIND_REPOSITORY=""
UB_TOOLS_CLONE_DIRECTORY=""
UB_TOOLS_REPOSITORY=""
SERVER_URL=""
SERVER_IP=""
EMAIL=""
MODULES=""
CONFIGS_DIRECTORY=""
SSL_CERT=""
SSL_KEY=""
SSL_CHAIN_FILE=""
FORCE_SSL=false
HTPASSWD=""
USER_NAME="vufind"
USER_GROUP="vufind"
CRONJOBS=""

source "$CONFIG_FILE"

##############################################################################
# user
##############################################################################

if id -u "$USER_NAME" >/dev/null 2>&1; then
	echo "User:                                                                    [ OK ]"
else
	echo "ERROR: user '${USER_NAME}' does not exist"
fi
if id -g "$USER_GROUP" >/dev/null 2>&1; then
	echo "Group:                                                                   [ OK ]"
else
	echo "ERROR: group '${USER_GROUP}' does not exist"
fi

##############################################################################
# mysql database
##############################################################################

DATABASE_CONFIG_FILE="$VUFIND_LOCAL_DIR/config/vufind/local_overrides/database.conf"
if [ -e "$DATABASE_CONFIG_FILE" ]; then
	echo "database.conf                                                            [ OK ]"
else
	echo "ERROR: '[...]/local/config/vufind/local_overrides/database.conf' is missing!"
fi

DATABASE_USERNAME=$(cat "$DATABASE_CONFIG_FILE" | sed 's/.*:\/\/\(.*\):.*@.*/\1/g')
DATABASE_PASSWORD=$(cat "$DATABASE_CONFIG_FILE" | sed 's/.*:\/\/.*:\(.*\)@.*/\1/g')
DATABASE_NAME=$(cat "$DATABASE_CONFIG_FILE" | sed 's/.*:\/\/.*:.*@.*\/\(.*\)"/\1/g')

if [[ $(mysql -u"$DATABASE_USERNAME" -p"$DATABASE_PASSWORD" -e ";") -eq 0 ]]; then
	echo "MySQL user and password                                                  [ OK ]"
else
	echo "ERROR: MySQL user or password is wrong!"
fi

RESULT=`mysql -u "$DATABASE_USERNAME" -p"$DATABASE_PASSWORD" --skip-column-names -e "SHOW DATABASES LIKE '$DATABASE_NAME'"`
if [[ "$RESULT" == "$DATABASE_NAME" ]]; then
	echo "MySQL Database                                                           [ OK ]"
else
	echo "ERROR: MySQL Database $DATABASE_NAME doesn't exist!"
fi

##############################################################################
# Clone repositories
##############################################################################

if [ -e "$VUFIND_HOME" ]; then
	echo "VUFIND_HOME                                                              [ OK ]"
else
	echo "'$VUFIND_HOME' is missing!                                               ERROR!"
fi
if [ -e "$VUFIND_LOCAL_DIR" ] && [ "$VUFIND_LOCAL_DIR" == "$VUFIND_HOME/local" ] ; then
	echo "VUFIND_LOCAL_DIR                                                         [ OK ]"
else
	echo "'$VUFIND_LOCAL_DIR' is missing!                                          ERROR!"
fi

UB_TOOLS_HOME=$(normalize_path "$VUFIND_HOME/../ub_tools")
if [ -e "$UB_TOOLS_HOME" ]; then
	echo "UB_TOOLS_HOME                                                            [ OK ]"
else
	echo "'$UB_TOOLS_HOME' is missing!                                             ERROR!"
fi

##############################################################################
# /var/lib/tuelib
##############################################################################

if [ -e "$UB_TOOLS_HOME/configs/ixtheo/cronjobs" ]; then
	echo "IxTheo configs subproject                                                [ OK ]"
else
	echo "IxTheo config subproject is missing!                                     ERROR!"
fi
if [ -e "$UB_TOOLS_HOME/configs/krimdok/cronjobs" ]; then
	echo "Krimdok configs subproject                                               [ OK ]"
else
	echo "Krimdok config subproject is missing!                                    ERROR!"
fi
if [ -e "/var/lib/tuelib/cronjobs/" ]; then
	echo "/var/lib/tuelib/cronjobs/                                                [ OK ]"
else
	echo "/var/lib/tuelib/cronjobs/ is missing!                                    ERROR!"
fi

if [ -h "/var/lib/tuelib/cronjobs" ]; then
	echo "Symlink of /var/lib/tuelib/cronjobs/                                     [ OK ]"
else
	echo "/var/lib/tuelib/cronjobs is not a Symlink!                               ERROR!"
fi

##############################################################################
# Configs
##############################################################################

if [ -e "$VUFIND_LOCAL_DIR/httpd-vufind.conf" ]; then
	echo "VUFIND_LOCAL_DIR/httpd-vufind.conf                                       [ OK ]"
else
	echo "VUFIND_LOCAL_DIR/httpd-vufind.conf is missing!                           ERROR!"
fi
if [ -h "/etc/apache2/conf-enabled/vufind2.conf" ] || [ -h "/etc/httpd/conf.d/vufind2.conf" ]; then
	echo "Symlink Apache httpd-vufind.conf                                         [ OK ]"
else
	echo "Symlink from Apache to httpd-vufind.conf is missing!                     ERROR!"
fi

if [ -e "$VUFIND_LOCAL_DIR/httpd-vufind-vhosts.conf" ]; then
	echo "VUFIND_LOCAL_DIR/httpd-vufind.conf                                       [ OK ]"
else
	echo "VUFIND_LOCAL_DIR/httpd-vufind.conf is missing!                           ERROR!"
fi
if [ -h "/etc/apache2/conf-enabled/vufind2-vhosts.conf" ] || [ -h "/etc/httpd/conf.d/vufind2-vhosts.conf" ]; then
	echo "Symlink Apache httpd-vufind.conf                                         [ OK ]"
else
	echo "Symlink from Apache to httpd-vufind.conf is missing!                     ERROR!"
fi