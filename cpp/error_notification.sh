#!/bin/bash
set -o nounset

if [ $# != 1 ]; then
    echo "Usage: $0 servicename"
    exit 1;
fi

service="$1"

/usr/local/bin/send_email --recipients=ixtheo-team@ub.uni-tuebingen.de --subject="Failure of ${service} on $(hostname --fqdn)" --message-body="$(systemctl status ${service})" --priority=very_high
