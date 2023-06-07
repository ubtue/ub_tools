#!/bin/bash
# Tool to extract titles from MARC file and autodetect language using lang_detection app

if [ $# != 3 ]; then
   echo "Usage $0 keibi.mrc lang_base_url association_output.txt"
   exit 1

fi

keibi_mrc="$1"
lang_base_url="$2"
output="$3"

marc_grep "${keibi_mrc}" '"245a"' control_number_and_traditional | \
    grep --extended-regexp '^KEI' | `#It turned out there are titles with multiple newlines and other garbageand other garbage  in it...` \
    cut  -f1,3- -d ':' | `#Throw way tag field`\
    tr '\n' '\0' | \
    tr -d '()\r`' | sed "s/['\"]//g" | `#Prevent parsing error`  \
    xargs -0 -I'{}' sh -c 'echo  "$@" | cut -f1 -d:; echo "$@" | cut -f2- -d: ' _ '{}' | `#split PPN and title` \
    sed --expression 's|<[^>]*>||g' | \
    tr -d ':' | \
    awk '{if (NR % 2 == 0) { system("curl --silent -X POST -H \"Content-Type: application/text\" \
         -d\""$0"\" \"http://'"${lang_base_url}"'/get_language\"");} else {print $0;}}' | \
    awk '{if (NR % 2 == 0) { print $0 | "jq --raw-output .lang"; close("jq --raw-output .lang"); } else {print $0}}' | \
    awk -F "---" '{if (NR % 2 == 0) { print $2 ":" $1} else { print $0 }}' | \
    sed -r --expression 's/(^\s+)|(\s+$)//g' | \
    paste -d: - - \
    > ${output}
