#!/bin/bash
set -o errexit -o nounset


if [[ $(/usr/local/bin/inifile_lookup /usr/local/var/lib/tuelib/boot_notification.conf "" notify) == "true" ]]; then
    # The following command does not return until the system is fully up and running!
    /usr/bin/systemctl is-system-running --wait

    /usr/local/bin/send_email --recipients=ixtheo-team@ub.uni-tuebingen.de --subject="$(hostname) just booted up!" \
                              --message-body="$(hostname --fqdn) was just booted." --priority=very_high
fi
