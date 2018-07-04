#!/bin/bash
# Upload given directory to server with dates
set -o errexit -o nounset


if [ $# != 5 ]; then
    echo "usage: $0 server username keyfile_path local_top_dir remote_top_dir"
    exit 1
fi

server=$1
username=$2
keyfile_path=$3
local_top_dir=$4
remote_top_dir=$5
date_dir=$(date +%y%m%d)

#Use a trick to see whether the date directory already exists (https://superuser.com/questions/850158/how-to-check-if-file-exists-in-remote-windows-from-local-linux-script) (180704)
#Create the directory if it doesn't

echo "df ${remote_top_dir}/${date_dir}" | sftp -oIdentityFile=${keyfile_path} ${username}@${server}
if [ $? -eq 0 ]; then
    sftp -oIdentityFile=${keyfile_path} ${username}@${server} <<EOF
    cd ${remote_top_dir}
    mkdir ${date_dir}
EOF
fi




# Upload the file 

sftp -oIdentityFile=${keyfile_path} ${username}@${server} <<EOF
lcd ${local_top_dir}
cd ${remote_top_dir}
cd ${date_dir}
put -r -p * .
EOF
