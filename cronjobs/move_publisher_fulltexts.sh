#!/bin/bash

# Move published fulltexts from the incoming WEBDAV directory to the network share
# where the fulltext machines can access them

readonly WEBDAV_ROOT_DIR="/usr/local/webdav/"
readonly FULLTEXT_INCOMING_ROOT_DIR="/mnt/ZE020150/FID-Entwicklung/fulltext/publisher_files"

cd ${WEBDAV_ROOT_DIR}
for publisher in "brill" "mohr"; do
	#https://serverfault.com/questions/775323/ignore-files-in-use-being-written-to-when-using-rsync (211018)
	find ${publisher} -type f -mmin +5 -print0 | rsync --archive --verbose --from0 --files-from=- \
	    --remove-source-files . ${FULLTEXT_INCOMING_ROOT_DIR}
done;

