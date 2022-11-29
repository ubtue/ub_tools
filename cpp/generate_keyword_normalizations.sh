#!/bin/bash
set -o errexit -o nounset -o histexpand


no_problems_found=1
function SendEmail {
    if [[ $no_problems_found -eq 0 ]]; then
        send_email --recipients="$email_address" --subject="$0 passed on $(hostname)" \
                   --message-body="The new keyword_normalizations.map file was successfully generated."
        exit 0
    else
        send_email --priority=high --recipients="$email_address" --subject="$0 failed on $(hostname)"  \
                   --message-body="$(printf '%q' "$(echo -e "Check /usr/local/var/log/tuefind/generate_keyword_normalizations.log for details.\n\n"" \
                                     "$(tail -20 /usr/local/var/log/tuefind/generate_keyword_normalizations.log)" \
                                     "'\n')")"
        exit 1
    fi
}
trap SendEmail EXIT


function Usage() {
    echo "Usage: $0 email_address"
    exit 1
}


# Argument processing
if [[ $# != 1 ]]; then
    Usage
fi
email_address=$1


/usr/local/bin/generate_keyword_normalizations localhost:8983 /tmp/new_keyword_normalization.map
mv /tmp/new_keyword_normalization.map /usr/local/var/lib/tuelib/keyword_normalization.map
no_problems_found=0
