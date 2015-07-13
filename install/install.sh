#!/bin/bash
#
#
set -o errexit -o nounset
BINARY_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
SCRIPT_DIR=$BINARY_DIR/scripts


show_help() {
  cat << EOF
Installs a 

USAGE: ${0##*/} [-v] CONFIG_FILE

-v               verbose

CONFIG_FILE      A config file containing the following definitions:
                    NAME="ixTheo"    # The name of this clone
                    CLONE_DIRECTORY="/path/to/clone"
                    REPOSITORY="https://github.com/ubtue/NAME.git"
                    SERVER_URL="localhost"
                    SERVER_IP="127.0.0.1"
                    EMAIL="support@localhost" 
                    MODULES="TueLib,ixTheo"   # Custom VuFind modules
                    SSL_CERT=""      # if set, provides https
                    SSL_KEY=""       # if set, provides https 
                    SSL_CHAIN_FILE=""
                    FORCE_SSL=false  # if true, redirects http to https 
                    HTPASSWD=""      # if set, provides password protection
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
	CONFIG_FILE=$2
else
	CONFIG_FILE=$1
fi

echo "VERBOSE: " $VERBOSE

# Check if confi
if  [[ ! -f $CONFIG_FILE ]] || [[ ! -r $CONFIG_FILE ]] ; then
  # if this is a relative path, make it absolute:
  $CONFIG_FILE=$(pwd)/$CONFIG_FILE 
  if  [[ ! -f $CONFIG_FILE ]] || [[ ! -r $CONFIG_FILE ]] ; then
    echo "ERROR: configuration file dosn't exist or isn't readable!"
    echo "Parameters: $*"
    show_help
    exit 1
  fi
fi


export VUFIND_HOME="/usr/local/vufind2"
export VUFIND_LOCAL_DIR="$VUFIND_HOME/local"
source $CONFIG_FILE

# Ask for root rights first. We need them later, but we ask for them now, so 
# the dosn't have to pay attention.
echo ""
echo 'This script needs three passwords. We ask for them now, so the script is able to run in background without interruption.'
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
$SCRIPT_DIR/create_user.sh

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_mysql.sh PASSWORD PASSWORD"
fi
$SCRIPT_DIR/create_mysql.sh $ROOT_PASSWORD $VUFIND_PASSWORD

if [[ -e $CLONE_DIRECTORY ]] ; then
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
  $SCRIPT_DIR/clone_git.sh $REPOSITORY $CLONE_DIRECTORY
fi

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "link_copy_to_vufind_home.sh $CLONE_DIRECTORY"
fi
$SCRIPT_DIR/link_copy_to_vufind_home.sh $CLONE_DIRECTORY

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_etc_profile.sh"
fi
$SCRIPT_DIR/create_etc_profile.sh

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_httpd_conf.sh $MODULES $FORCE_SSL"
fi
$SCRIPT_DIR/create_httpd_conf.sh $MODULES $FORCE_SSL

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_httpd_vhost_conf.sh $SERVER_URL $SERVER_IP $SSL_CERT $SSL_KEY"
fi
$SCRIPT_DIR/create_httpd_vhost_conf.sh $SERVER_IP $SERVER_URL $SSL_CERT $SSL_KEY $SSL_CHAIN_FILE

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "link_httpd_config.sh"
fi
$SCRIPT_DIR/link_httpd_config.sh

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_private_server_config.sh $SERVER_URL $SERVER_IP $EMAIL PASSWORD"
fi
$SCRIPT_DIR/create_private_server_config.sh $SERVER_IP $SERVER_URL $EMAIL $VUFIND_PASSWORD

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_start_script.sh $CLONE_DIRECTORY"
fi
$SCRIPT_DIR/create_start_script.sh $CLONE_DIRECTORY

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "set_privileges.sh"
fi
$SCRIPT_DIR/set_privileges.sh

if [[ $HTPASSWD ]] ; then
  if [[ "$VERBOSE" == true ]] ; then
    echo ""
    echo ""
    echo "create_htpasswd_protection.sh $NAME $HTPASSWD"
  fi
  $SCRIPT_DIR/create_htpasswd_protection.sh $NAME $HTPASSWD
fi

##############################################################################
# RESTART SERVER
##############################################################################

  echo ""
  echo "DONE"
  