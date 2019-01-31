#!/bin/bash
set -e


cat bnb.yaz \
    | sed s/YYYYMM/$(date +'%Y%m' -d 'last month') \
    | yaz-client \
    | sed --silent '/error/{q1}' # Quit w/ exit code 1 if the output of yaz-client contains the string "error" 
