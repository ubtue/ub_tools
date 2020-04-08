#!/bin/bash
set -o errexit -o nounset


function Usage() {
    echo "Usage: $0 email_address"
    exit 1
}
if [[ $# != 1 ]]; then
    Usage
fi


no_problems_found=1
function SendEmail {
    if [[ $no_problems_found -eq 0 ]]; then
        send_email --priority=low --recipients="$email_address" --subject="$0 passed on $(hostname)" --message-body="No problems were encountered."
        exit 0
    else
        send_email --priority=high --recipients="$email_address" --subject="$0 failed on $(hostname)"  \
                   --message-body="$(printf '%q' "$(echo -e "Check /usr/local/var/log/tuefind/generate_subscription_packets.log for details.\n\n"" \
                                     "$(tail -20 /usr/local/var/log/tuefind/generate_subscription_packets.log)" \
                                     "'\n')")"
        exit 1
    fi
}
trap SendEmail EXIT


readonly email_address=$1


generate_subscription_packets /usr/local/var/lib/tuelib/relbib_packets.conf /tmp/relbib_bundles.out
mv /tmp/relbib_bundles.out /usr/local/var/lib/tuelib/journal_alert_bundles.conf

no_problems_found=0
