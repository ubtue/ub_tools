#!/bin/bash
#
#
set -o errexit -o nounset
BINARY_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SCRIPT_DIR=$BINARY_DIR/scripts


show_help() {
  cat << EOF
Installs a 

USAGE: ${0##*/} [-v] CONFIG_FILE

-v               verbose

CONFIG_FILE     A config file for this installation.
                See $(pwd)/template.conf
EOF
}

VERBOSE=false
CONFIG_FILE=""

if [ "$#" -ne 1 ] || [ "$#" -eq 2 ] && [ "$1" != "-v" ] ; then
  echo "ERROR: Illegal number of parameters!"
  echo "Parameters: $*"
  show_help
  exit 1
elif [ "$#" -eq 2 ] && [ "$1" == "-v" ] ; then
	VERBOSE=true
	CONFIG_FILE="$2"
else
	CONFIG_FILE="$1"
fi

# Check if config exists and is readable
if  [[ ! -f "$CONFIG_FILE" ]] || [[ ! -r "$CONFIG_FILE" ]] ; then
  # if this is a relative path, make it absolute:
  CONFIG_FILE="$(pwd)/$CONFIG_FILE"
  if  [[ ! -f "$CONFIG_FILE" ]] || [[ ! -r "$CONFIG_FILE" ]] ; then
    echo "ERROR: configuration file dosn't exist or isn't readable!"
    echo "Parameters: $*"
    show_help
    exit 1
  fi
fi


export VUFIND_HOME="/usr/local/vufind2"
export VUFIND_LOCAL_DIR="$VUFIND_HOME/local"

NAME=""
CLONE_DIRECTORY=""
REPOSITORY=""
SERVER_URL=""
SERVER_IP=""
EMAIL=""
MODULES=""
SSL_CERT=""
SSL_KEY=""
SSL_CHAIN_FILE=""
FORCE_SSL=false
HTPASSWD=""

source "$CONFIG_FILE"

# Ask for root rights first. We need them later, but we ask for them now, so 
# the dosn't have to pay attention.
echo ""
echo 'This script needs three passwords. We ask for them now, so the script is able to run in the background without interruption.'
echo ""
echo "First: if you aren't root, we need root rights to modify some configuration files of Apache, MySQL, to create and modify VUFIND_HOME and to create the VuFind user."
sudo echo "Root rights granted"
echo ""

##############################################################################
#  Ask for passwords
##############################################################################

echo "Second: the password of the MySQL root user is needed to (re-)create the database of VuFind."
read -s -p "Enter Password: " ROOT_PASSWORD
echo ""

echo "Third: the password of the MySQL VuFind user is needed to (re-)create the MySQL VuFind user."
read -s -p "Enter Password: " VUFIND_PASSWORD
echo ""

##############################################################################
# Write configurations
##############################################################################

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_user.sh"
fi
"$SCRIPT_DIR/create_user.sh"

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_mysql.sh PASSWORD PASSWORD"
fi
"$SCRIPT_DIR/create_mysql.sh" "$ROOT_PASSWORD" "$VUFIND_PASSWORD"

if [[ -e "$CLONE_DIRECTORY" ]] ; then
  if [[ "$VERBOSE" == true ]] ; then
  	echo ""
  	echo ""
  	echo "Didn't clone git repository. Directory exists"
  fi
else
  if [[ "$VERBOSE" == true ]] ; then
    echo ""
    echo ""
    echo "clone_git.sh $REPOSITORY $CLONE_DIRECTORY"
  fi
  "$SCRIPT_DIR/clone_git.sh" "$REPOSITORY" "$CLONE_DIRECTORY"
fi

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "link_copy_to_vufind_home.sh $CLONE_DIRECTORY"
fi
"$SCRIPT_DIR/link_copy_to_vufind_home.sh" "$CLONE_DIRECTORY"

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_etc_profile.sh"
fi
"$SCRIPT_DIR/create_etc_profile.sh"

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_httpd_conf.sh $MODULES $FORCE_SSL"
fi
"$SCRIPT_DIR/create_httpd_conf.sh" "$MODULES" "$FORCE_SSL"

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_httpd_vhost_conf.sh $SERVER_URL $SERVER_IP $SSL_CERT $SSL_KEY"
fi
"$SCRIPT_DIR/create_httpd_vhost_conf.sh" "$SERVER_IP" "$SERVER_URL" "$SSL_CERT" "$SSL_KEY" "$SSL_CHAIN_FILE"

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "link_httpd_config.sh"
fi
"$SCRIPT_DIR/link_httpd_config.sh"

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_private_server_config.sh $SERVER_URL $SERVER_IP $EMAIL PASSWORD"
fi
"$SCRIPT_DIR/create_private_server_config.sh" "$SERVER_IP" "$SERVER_URL" "$EMAIL" "$VUFIND_PASSWORD"


if [[ "$VUFIND_HOME" != "$CLONE_DIRECTORY" ]] ; then
  if [[ "$VERBOSE" == true ]] ; then
    echo ""
    echo ""
    echo "create_start_script.sh $CLONE_DIRECTORY"
  fi
  "$SCRIPT_DIR/create_start_script.sh" "$CLONE_DIRECTORY"
else
  if [[ "$VERBOSE" == true ]] ; then
    echo ""
    echo ""
    echo "No start script! This is a single copy installation. If you want multible copies, DO NOT use '$VUFIND_HOME' as cloning directory."
  fi 
fi

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "set_privileges.sh"
fi
"$SCRIPT_DIR/set_privileges.sh"

if [ -x "/bin/systemctl"] ; then
	if [[ "$VERBOSE" == true ]] ; then
    echo ""
    echo ""
    echo "create_systemd_conf.sh"
  fi
  "$SCRIPT_DIR/create_systemd_conf.sh"
else
  if [[ "$VERBOSE" == true ]] ; then
    echo ""
    echo ""
    echo "create_upstart_conf.sh"
  fi
  "$SCRIPT_DIR/create_upstart_conf.sh"
fi

if [[ "$HTPASSWD" ]] ; then
  if [[ "$VERBOSE" == true ]] ; then
    echo ""
    echo ""
    echo "create_htpasswd_protection.sh $NAME $HTPASSWD"
  fi
  "$SCRIPT_DIR/create_htpasswd_protection.sh" "$NAME" "$HTPASSWD"
fi

##############################################################################
# RESTART SERVER
##############################################################################

"$CLONE_DIRECTORY/start-vufind.sh"

  echo ""
  echo "DONE"
  