#!/bin/bash
# We intentionally do not use errexit here, because we would like to create the email body
# based on the result of `systemctl is-system-running`, even if its exit code is not 0.
set -o nounset


if [[ $(/usr/local/bin/inifile_lookup /usr/local/var/lib/tuelib/boot_notification.conf "" notify) == "true" ]]; then
    if [[ $(systemctl is-system-running) == "degraded" ]]; then
        BODY=$(systemctl list-units --state=failed)
    else
        BODY="All units were successfully started."
    fi
    /usr/local/bin/send_email --recipients=ixtheo-team@ub.uni-tuebingen.de --subject="$(hostname --fqdn) just booted up!" \
                              --message-body="$BODY" --priority=very_high
fi
