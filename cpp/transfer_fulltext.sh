#!/bin/bash
# Upload given directory to server with dates
set -o errexit -o nounset


if [ $# -lt 6 ]; then
    echo "usage: $0 server username keyfile_path local_top_dir remote_top_dir directory1 directory2 ..."
    exit 1
fi

server=$1
shift
username=$1
shift
keyfile_path=$1
shift
local_top_dir=$1
shift
remote_top_dir=$1
shift
date_dir=$(date +%y%m%d)
dirs_to_transfer=$@

#Use a trick to see whether the date directory already exists (https://superuser.com/questions/850158/how-to-check-if-file-exists-in-remote-windows-from-local-linux-script) (180704)
#Create the directory if it doesn't

function CreateDirIfNotExists {
    dir_to_test=$1
    echo "df ${dir_to_test}" | sftp -oIdentityFile=${keyfile_path} ${username}@${server}
    if [ $? -eq 0 ]; then
        sftp -oIdentityFile=${keyfile_path} ${username}@${server} <<-EOF
		cd ${remote_top_dir}
		mkdir ${date_dir}
		EOF
    fi
}

function TransferDirectory {
    dir_to_transfer=$1
    sftp -oIdentityFile=${keyfile_path} ${username}@${server} <<-EOF
	lcd ${local_top_dir}
	cd ${remote_top_dir}
	cd ${date_dir}
	put -r -p  ${dir_to_transfer}
	EOF
}


# Create the top level directories
CreateDirIfNotExists "${remote_top_dir}/${date_dir}"

# Upload all the directories that contain 7z files
for dir_to_transfer in $dirs_to_transfer
do
    TransferDirectory ${dir_to_transfer}
done

