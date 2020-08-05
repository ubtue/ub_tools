#!/bin/bash
# Script to upload files safely to the BSZ FTP server
#
# Note that the used ftp command will not terminate if an error occurs.
# This script will be replaced by a python script soon.

set -e

if [ $# -ne 2 ]; then
    echo "Usage: $0 local_file remote_folder_path"
    exit 1
fi

input_path=$1
filename=$(basename "$input_path")
filename_tmp="${filename%.*}.tmp"
remote_path=$2

conf_path=/usr/local/var/lib/tuelib/cronjobs/BSZ.conf
host=$(inifile_lookup --suppress-newline "${conf_path}" FTP host)
username=$(inifile_lookup --suppress-newline "${conf_path}" FTP username)
password=$(inifile_lookup --suppress-newline "${conf_path}" FTP password)
ftp -invp "${host}" <<EOF
user ${username} ${password}
cd ${remote_path}
binary
put ${input_path} ${filename_tmp}
rename ${filename_tmp} ${filename}
bye
EOF


