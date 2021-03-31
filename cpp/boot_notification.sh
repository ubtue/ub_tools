#!/bin/bash
set -o errexit -o nounset


if [[ $(/usr/local/bin/inifile_lookup /usr/local/var/lib/tuelib/boot_notification.conf "" notify) == "true" ]]; then
    /usr/local/bin/send_email --recipients=ixtheo-team@ub.uni-tuebingen.de --subject="$(hostname) just booted up!" \
                              --message-body="$(hostname --fqdn) was just booted." --priority=very_high
fi
