#!/bin/bash
set -o nounset

if [ $# != 1 ]; then
    echo "Usage: $0 servicename"
    exit 1;
fi

service="$1"
# A combination of Restart=on-failure and trigger a service with OnFailure
# no longer works. Thus evaluate $SERVICE_RESULT to inform only about unexpected
# restarts:
# c.f. https://serverfault.com/questions/876233/how-to-send-an-email-if-a-systemd-service-is-restarted#876254 (231213)

if [ ${SERVICE_RESULT} == "success" ]; then
  exit 0
fi

/usr/local/bin/send_email --recipients=ixtheo-team@ub.uni-tuebingen.de --subject="Restart after error of ${service} on $(hostname --fqdn)" --message-body="$(systemctl status ${service})" --priority=very_high
