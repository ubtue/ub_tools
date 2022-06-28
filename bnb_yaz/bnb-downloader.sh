#!/bin/bash


BIN=/usr/local/bin
EMAIL=ixtheo-team@ub.uni-tuebingen.de
MARC_FILENAME=bnb-$(date +%y%m%d).mrc


cd /usr/local/ub_tools/bnb_yaz
rm --force "$MARC_FILENAME"


username_password=$(</usr/local/var/lib/tuelib/bnb_username_password.conf)
escaped_username_password=$(echo $username_password | sed 's;/;\\/;g') # Escape slashes for sed in the next statement.
cat bnb.yaz \
    | sed "s/USERNAME_PASSWORD/$escaped_username_password/; s/YYYYMM/$(date +'%Y%m' -d 'last month')/; s/MARC_FILENAME/$MARC_FILENAME/" \
    | yaz-client \
    | sed '/error/{q1}' > bnb-downloader.log # Quit w/ exit code 1 if the output of yaz-client contains the string "error"

if [[ $? == 0 ]]; then
    "$BIN/send_email" --priority=medium --recipients=$EMAIL --subject="BNB Data Download Succeeded" \
                      --message-body="Number of downloaded records: $(marc_size $MARC_FILENAME)"
else
    "$BIN/send_email" --priority=high --recipients=$EMAIL --subject="BNB Data Download Failed" --message-body="See bnb-downloader.log"
fi
