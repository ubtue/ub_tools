#!/bin/bash
# 
# Config files for the marc pipeline scripts are globally stored 
# on //sn00.zdv.uni-tuebingen.de/ZE020150/IT-Abteilung/02_Projekte/08_iXTheo_2_0/04_Konfigurationen/
# in the cronjobs subdirectory
# The ZE020150 drive must be mounted as /mnt/ZE020150.
# We will copy configuration scripts to the CONFIGS_DIRECTORY/cronjobs

set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

show_help() {
    cat << EOF
Links some configs.

USAGE: ${0##*/} CONFIGS_ORIGIN CONFIGS_DIRECTORY 

CONFIGS_ORIGIN	     Directory holding configuration files for the marc pipeline 
CONFIGS_DIRECTORY    The directory holding the configs after installation

EOF
}

copy_config_files(){

   CONFIGS_ORIGIN=$1
   CONFIGS_DIRECTORY=$2

   if [[ ! -d $CONFIGS_ORIGIN ]] ; then
	
	echo "Could not find directory $CONFIGS_ORIGIN"
	exit 1
   fi

   # 
   
   if [[ ! -d $CONFIGS_DIRECTORY ]]; then
	mkdir -p $CONFIGS_DIRECTORY
   fi

   cp --archive --recursive "$CONFIGS_ORIGIN/cronjobs/" "$CONFIGS_DIRECTORY"

}

if [ "$#" -ne 2 ]; then
  echo "ERROR: Illegal number of parameters!"
  echo "Parameters: $*"
  show_help
  exit 1
fi


CONFIGS_ORIGIN=$1
CONFIGS_DIRECTORY=$2

copy_config_files $CONFIGS_ORIGIN $CONFIGS_DIRECTORY 


