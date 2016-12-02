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

VERBOSE=""
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
    echo "ERROR: configuration file doesn't exist or isn't readable!"
    echo "Parameters: $*"
    show_help
    exit 1
  fi
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
SYSTEM_TYPE=""

if [[  "$CONFIG_FILE" =~ krimdok ]]; then
    SYSTEM_TYPE="krimdok"
elif [[  "$CONFIG_FILE" =~ ixtheo ]]; then
    SYSTEM_TYPE="ixtheo"
else
    echo 'ERROR: config file name must contain either "krimdok" or "ixtheo"!'
    exit 1
fi


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

if [[ "$VERBOSE" ]] ; then
  echo ""
  echo ""
  echo "create_user.sh $USER_NAME $USER_GROUP"
fi
"$SCRIPT_DIR/create_user.sh" "$USER_NAME" "$USER_GROUP"

##############################################################################
# Clone repositories
##############################################################################

if [[ -e "$VUFIND_CLONE_DIRECTORY" ]] ; then
  if [[ "$VERBOSE" ]] ; then
    echo ""
    echo ""
    echo "Didn't clone $VUFIND_REPOSITORY repository. Directory exists"
  fi
else
  if [[ "$VERBOSE" ]] ; then
    echo ""
    echo ""
    echo "clone_git.sh $VUFIND_REPOSITORY $VUFIND_CLONE_DIRECTORY"
  fi
  "$SCRIPT_DIR/clone_git.sh" "$VUFIND_REPOSITORY" "$VUFIND_CLONE_DIRECTORY"
fi

##############################################################################

if [[ -e "$UB_TOOLS_CLONE_DIRECTORY" ]] ; then
  if [[ "$VERBOSE" ]] ; then
    echo ""
    echo ""
    echo "Didn't clone $UB_TOOLS_REPOSITORY repository. Directory exists"
  fi
else
  if [[ "$VERBOSE" ]] ; then
    echo ""
    echo ""
    echo "clone_git.sh $UB_TOOLS_REPOSITORY $UB_TOOLS_CLONE_DIRECTORY"
  fi
  "$SCRIPT_DIR/clone_git.sh" "$UB_TOOLS_REPOSITORY" "$UB_TOOLS_CLONE_DIRECTORY"
fi

##############################################################################

if [[ "$VUFIND_HOME" != "$VUFIND_CLONE_DIRECTORY" ]] ; then
  if [[ "$VERBOSE" ]] ; then
    echo ""
    echo ""
    echo "link_clones.sh $VUFIND_CLONE_DIRECTORY $UB_TOOLS_CLONE_DIRECTORY"
  fi
  "$SCRIPT_DIR/link_clones.sh" "$VUFIND_CLONE_DIRECTORY" "$UB_TOOLS_CLONE_DIRECTORY"
else
  if [[ "$VERBOSE" ]] ; then
    echo ""
    echo ""
    echo "No symbolic linking of copy directory to '$VUFIND_HOME'! This is a single copy installation. If you want multible copies, DO NOT use '$VUFIND_HOME' as cloning directory."
  fi
fi

##############################################################################
# Write configurations
##############################################################################

if [[ "$VERBOSE" ]] ; then
  echo ""
  echo ""
  echo "create_etc_profile.sh"
fi
"$SCRIPT_DIR/create_etc_profile.sh"

##############################################################################

if [[ "$VERBOSE" ]] ; then
  echo ""
  echo ""
  echo "create_httpd_conf.sh $MODULES $FORCE_SSL"
fi
"$SCRIPT_DIR/create_httpd_conf.sh" "$MODULES" "$FORCE_SSL"

##############################################################################

if [[ "$VERBOSE" ]] ; then
  echo ""
  echo ""
  echo "create_httpd_vhost_conf.sh $SERVER_URL $SERVER_IP $SSL_CERT $SSL_KEY"
fi
"$SCRIPT_DIR/create_httpd_vhost_conf.sh" "$SERVER_IP" "$SERVER_URL" "$SSL_CERT" "$SSL_KEY" "$SSL_CHAIN_FILE"

##############################################################################

if [[ "$VERBOSE" ]] ; then
  echo ""
  echo ""
  echo "create_private_server_config.sh $SERVER_URL $SERVER_IP $EMAIL PASSWORD"
fi
"$SCRIPT_DIR/create_private_server_config.sh" "$SERVER_IP" "$SERVER_URL" "$EMAIL" "$VUFIND_PASSWORD"

##############################################################################

if [[ "$VERBOSE" ]] ; then
  echo ""
  echo ""
  echo "create_cronjobs.sh $CRONJOBS"
fi
"$SCRIPT_DIR/create_cronjobs.sh" "$CRONJOBS"

##############################################################################

if [[ "$HTPASSWD" ]] ; then
  if [[ "$VERBOSE" ]] ; then
    echo ""
    echo ""
    echo "create_htpasswd_protection.sh $NAME $HTPASSWD"
  fi
  "$SCRIPT_DIR/create_htpasswd_protection.sh" "$NAME" "$HTPASSWD"
fi

##############################################################################
# Set Privileges
##############################################################################

if [[ "$VERBOSE" ]] ; then
  echo ""
  echo ""
  echo "set_privileges.sh $VUFIND_CLONE_DIRECTORY $USER_NAME $USER_GROUP $SYSTEM_TYPE"
fi
"$SCRIPT_DIR/set_privileges.sh" "$VUFIND_CLONE_DIRECTORY" "$USER_NAME" "$USER_GROUP" "$SYSTEM_TYPE"

##############################################################################
# Linking
##############################################################################

if [[ "$VERBOSE" ]] ; then
  echo ""
  echo ""
  echo "link_httpd_config.sh"
fi
"$SCRIPT_DIR/link_httpd_config.sh"

##############################################################################

if [[ "$VERBOSE" ]] ; then
  echo ""
  echo ""
  echo "link_configs.sh $CONFIGS_DIRECTORY"
fi
"$SCRIPT_DIR/link_configs.sh" "$CONFIGS_DIRECTORY" "$SYSTEM_TYPE"

##############################################################################

if [[ "$VERBOSE" ]] ; then
  echo ""
  echo ""
  echo "link_python2.sh"
fi
"$SCRIPT_DIR/link_python2.sh"

##############################################################################
# Linking
##############################################################################

if [[ "$VUFIND_HOME" != "$VUFIND_CLONE_DIRECTORY" ]] ; then
  if [[ "$VERBOSE" ]] ; then
    echo ""
    echo ""
    echo "create_start_script.sh $VUFIND_CLONE_DIRECTORY"
  fi
  "$SCRIPT_DIR/create_start_script.sh" "$VUFIND_CLONE_DIRECTORY"
else
  if [[ "$VERBOSE" ]] ; then
    echo ""
    echo ""
    echo "No start script! This is a single copy installation. If you want multiple copies, DO NOT use '$VUFIND_HOME' as cloning directory."
  fi 
fi

##############################################################################
# Compile
##############################################################################

if [[ "$VERBOSE" ]] ; then
  echo ""
  echo ""
  echo "make_install_ub_tools.sh"
fi
"$SCRIPT_DIR/make_install_ub_tools.sh"

#############################################################################
# Insert muttrc
#############################################################################

if [[ "$VERBOSE" ]] ; then
  echo ""
  echo ""
  echo "create_muttrc.sh"
fi
"$SCRIPT_DIR/create_muttrc.sh" "$EMAIL"

##############################################################################
# Setup mysql database
##############################################################################

if [[ "$VERBOSE" ]] ; then
  echo ""
  echo ""
  echo "create_mysql.sh PASSWORD PASSWORD"
fi
"$SCRIPT_DIR/create_mysql.sh" "$ROOT_PASSWORD" "$VUFIND_PASSWORD"

##############################################################################
# Configure Full-Text Indexing
##############################################################################

if [[ $SYSTEM_TYPE == "krimdok" ]]; then
  if [[ "$VERBOSE" ]] ; then
    echo ""
    echo ""
    echo "$SCRIPT_DIR/create_fulltext_ini.sh $UB_TOOLS_CLONE_DIRECTORY"
  fi
  "$SCRIPT_DIR/create_fulltext_ini.sh" "$UB_TOOLS_CLONE_DIRECTORY"
fi

##############################################################################
# start server
##############################################################################

if [[ -e "/etc/systemd/system/httpd.conf" ]]; then
  if [[ "$VERBOSE" ]] ; then
    echo ""
    echo ""
    echo "create_systemd_conf.sh"
  fi
  "$SCRIPT_DIR/create_systemd_conf.sh"
else
  if [[ "$VERBOSE" ]] ; then
    echo ""
    echo ""
    echo "create_upstart_conf.sh"
  fi
  "$SCRIPT_DIR/create_upstart_conf.sh"
fi

##############################################################################

if [[ -e "/etc/systemd/system/httpd.conf" ]]; then
  if [[ "$VERBOSE" ]] ; then
    echo ""
    echo ""
    echo "Start server with SystemD"
  fi
  systemctl restart httpd.service
  systemctl restart mariadb.service
  systemctl restart vufind.service
else
  if [[ "$VERBOSE" ]] ; then
    echo ""
    echo ""
    echo "Start server with upstart"
  fi

  service apache2 restart
  service mysql restart
  service vufind restart
fi

#############################################################################
# Compile CSS
#############################################################################

cd "$VUFIND_HOME"
php "util/cssBuilder.php"
cd -


echo ""
echo "DONE"
echo "Please restart Apache- and $NAME-Server"
  
