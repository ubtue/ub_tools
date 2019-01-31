#!/bin/bash
set -e

username_password=$(<bnb_username_password.conf)
cat bnb.yaz \
    | sed "s/USERNAME_PASSWORD/$username_password/; s/YYYYMM/$(date +'%Y%m' -d 'last month')" \
    | yaz-client \
    | sed --silent '/error/{q1}' # Quit w/ exit code 1 if the output of yaz-client contains the string "error" 
