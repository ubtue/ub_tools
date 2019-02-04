#!/bin/bash


BIN=/usr/local/bin
EMAIL=johannes.ruscheinski@ub.uni-tuebingen.de


cd /usr/local/ub_tools/bnb_yaz


username_password=$(<bnb_username_password.conf)
cat bnb.yaz \
    | sed "s/USERNAME_PASSWORD/$username_password/; s/YYYYMM/$(date +'%Y%m' -d 'last month')/" \
    | yaz-client \
    | sed '/error/{q1}' > bnb-downloader.log # Quit w/ exit code 1 if the output of yaz-client contains the string "error"

if [[ $? == 0 ]]; then
    "$BIN/send_email" --priority=normal --recipients=$EMAIL --subject="BNB Data Download Succeeded" --message-body="See bnb-downloader.log"
else
    "$BIN/send_email" --priority=high --recipients=$EMAIL --subject="BNB Data Download Failed" \
                      --message-body="Number of downloaded records:$(cat bnb-downloader.log|grep 'Records: '|cut -d: -f2)"
fi
