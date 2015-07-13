  #! /bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Writes the start script for a specific VuFind clone.
#
set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )


show_help() {
  cat << EOF
Writes the start script for a specific VuFind clone.

USAGE: ${0##*/} CLONE_DIRECTORY_PATH

CLONE_DIRECTORY_PATH     The path to the directory of the
                         specific VuFind clone.

EOF
}


if [ "$#" -ne 1 ]; then
  echo "ERROR: Illegal number of parameters!"
  echo "Parameters: $*"
  show_help
  exit 1
fi

LOCAL_COPY_DIRECTORY=$1
START_SCRIPT="$LOCAL_COPY_DIRECTORY/start_vufind.sh"

sh -c "> $START_SCRIPT"
# paths
sh -c "echo export JAVA_HOME=\"/usr/lib/jvm/default-java\"        >> $START_SCRIPT"
sh -c "echo export VUFIND_HOME=\"$VUFIND_HOME\"                   >> $START_SCRIPT"
sh -c "echo export VUFIND_LOCAL_DIR=\"$VUFIND_LOCAL_DIR\"         >> $START_SCRIPT"
# stop server
sh -c "echo '/usr/local/vufind2/vufind.sh stop'                   >> $START_SCRIPT"
sh -c "echo 'sudo apache2ctl stop'                                >> $START_SCRIPT"
sh -c "echo 'sleep 2'                                             >> $START_SCRIPT"
# link default location
sh -c "echo sudo ln -sfT $LOCAL_COPY_DIRECTORY /usr/local/vufind2 >> $START_SCRIPT"
# start server
sh -c "echo 'sudo apache2ctl start'                               >> $START_SCRIPT"
sh -c "echo '/usr/local/vufind2/vufind.sh start'                  >> $START_SCRIPT"
sudo chmod ug+x $START_SCRIPT
