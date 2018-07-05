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

#Use a trick to see whether the date directory already exists
#(https://superuser.com/questions/850158/how-to-check-if-file-exists-in-remote-windows-from-local-linux-script) (180704)
#Create the directory if it doesn't
function CreateDirIfNotExists {
    remote_top_dir=$1
    dir_to_test=$2
    # Needed since otherwise we would be aborted because of our test
    set +o errexit
    echo "df ${dir_to_test}" | sftp -b - -oIdentityFile=${keyfile_path} ${username}@${server}:${remote_top_dir}
    if [ $? -ne 0 ]; then
        sftp -oIdentityFile=${keyfile_path} ${username}@${server}:${remote_top_dir} <<-EOF
	mkdir ${dir_to_test}
	EOF
    echo "Created directory ${dir_to_test}"
    else
         echo "Directory ${dir_to_test} already present"
    fi
    set -o errexit
}


function TransferDirectory {
    local_top_dir=$1
    remote_top_dir=$2
    dir_to_transfer=$3
    sftp -oIdentityFile=${keyfile_path} ${username}@${server} <<-EOF
	lcd ${local_top_dir}
	cd ${remote_top_dir}
	cd ${date_dir}
	put -r -p  ${dir_to_transfer}
	EOF
}


# Create the top level directories
CreateDirIfNotExists "${remote_top_dir}" "${date_dir}"

# Upload all the directories that contain 7z files
for dir_to_transfer in $dirs_to_transfer
do
    TransferDirectory ${local_top_dir} ${remote_top_dir} ${dir_to_transfer}
done
