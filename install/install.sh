#!/bin/bash
#
#
set -o errexit -o nounset
BINARY_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SCRIPT_DIR=$BINARY_DIR/scripts

# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root to modify some configuration files of Apache, MySQL, to create and modify VUFIND_HOME and to create the VuFind user."  1>&2
   exit 1
fi

show_help() {
  cat << EOF
Installs a specialized copy of VuFind (like ixTheo or VuFind) using a
configuration file 

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
  # Maybe it is a path relativ to the current working directory. Just try it...
  CONFIG_FILE="$(pwd)/$CONFIG_FILE"
  if  [[ ! -f "$CONFIG_FILE" ]] || [[ ! -r "$CONFIG_FILE" ]] ; then
    echo "ERROR: configuration file dosen't exist or isn't readable!"
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
USER_NAME="vufind"
USER_GROUP="vufind"
CRONJOBS=""

source "$CONFIG_FILE"

##############################################################################
#  Ask for passwords
##############################################################################

echo "The password of the MySQL root user is needed to (re-)create the database of VuFind."
read -s -p "Enter Password: " ROOT_PASSWORD
echo ""

echo "The password of the MySQL VuFind user is needed to (re-)create the MySQL VuFind user."
read -s -p "Enter Password: " VUFIND_PASSWORD
echo ""

##############################################################################
# Write configurations
##############################################################################

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_user.sh $USER_NAME $USER_GROUP"
fi
"$SCRIPT_DIR/create_user.sh" "$USER_NAME" "$USER_GROUP"

##############################################################################

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_mysql.sh PASSWORD PASSWORD"
fi
"$SCRIPT_DIR/create_mysql.sh" "$ROOT_PASSWORD" "$VUFIND_PASSWORD"

##############################################################################

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

##############################################################################

if [[ "$VUFIND_HOME" != "$CLONE_DIRECTORY" ]] ; then
  if [[ "$VERBOSE" == true ]] ; then
    echo ""
    echo ""
    echo "link_copy_to_vufind_home.sh $CLONE_DIRECTORY"
  fi
  "$SCRIPT_DIR/link_copy_to_vufind_home.sh" "$CLONE_DIRECTORY"
else
  if [[ "$VERBOSE" == true ]] ; then
    echo ""
    echo ""
    echo "No symbolic linking of copy directory to '$VUFIND_HOME'! This is a single copy installation. If you want multible copies, DO NOT use '$VUFIND_HOME' as cloning directory."
  fi
fi
##############################################################################

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "link_python2.sh"
fi
"$SCRIPT_DIR/link_python2.sh"

##############################################################################

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_etc_profile.sh"
fi
"$SCRIPT_DIR/create_etc_profile.sh"

##############################################################################

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_httpd_conf.sh $MODULES $FORCE_SSL"
fi
"$SCRIPT_DIR/create_httpd_conf.sh" "$MODULES" "$FORCE_SSL"

##############################################################################

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_httpd_vhost_conf.sh $SERVER_URL $SERVER_IP $SSL_CERT $SSL_KEY"
fi
"$SCRIPT_DIR/create_httpd_vhost_conf.sh" "$SERVER_IP" "$SERVER_URL" "$SSL_CERT" "$SSL_KEY" "$SSL_CHAIN_FILE"

##############################################################################

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "link_httpd_config.sh"
fi
"$SCRIPT_DIR/link_httpd_config.sh"

##############################################################################

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_private_server_config.sh $SERVER_URL $SERVER_IP $EMAIL PASSWORD"
fi
"$SCRIPT_DIR/create_private_server_config.sh" "$SERVER_IP" "$SERVER_URL" "$EMAIL" "$VUFIND_PASSWORD"

##############################################################################

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
    echo "No start script! This is a single copy installation. If you want multiple copies, DO NOT use '$VUFIND_HOME' as cloning directory."
  fi 
fi

##############################################################################

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "set_privileges.sh $CLONE_DIRECTORY $USER_NAME $USER_GROUP"
fi
"$SCRIPT_DIR/set_privileges.sh" "$CLONE_DIRECTORY" "$USER_NAME" "$USER_GROUP"

##############################################################################

if [ -x "/bin/systemctl" ] ; then
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

##############################################################################

if [[ "$VERBOSE" == true ]] ; then
  echo ""
  echo ""
  echo "create_cronjobs.sh $CRONJOBS"
fi
"$SCRIPT_DIR/create_cronjobs.sh" "$CRONJOBS"

##############################################################################

if [[ "$HTPASSWD" ]] ; then
  if [[ "$VERBOSE" == true ]] ; then
    echo ""
    echo ""
    echo "create_htpasswd_protection.sh $NAME $HTPASSWD"
  fi
  "$SCRIPT_DIR/create_htpasswd_protection.sh" "$NAME" "$HTPASSWD"
fi

##############################################################################
# start server
##############################################################################

if [ -x "/bin/systemctl" ] ; then
  if [[ "$VERBOSE" == true ]] ; then
    echo ""
    echo ""
    echo "Start server with SystemD"
  fi
  systemctl restart httpd.service
  systemctl restart mariadb.service
  systemctl start vufind.service
else
  if [[ "$VERBOSE" == true ]] ; then
    echo ""
    echo ""
    echo "Start server with upstart"
  fi

  service apache2 restart
  service mysql restart
  service vufind start
fi


echo ""
echo "DONE"
echo "Please restart Apache- and $NAME-Server"
  