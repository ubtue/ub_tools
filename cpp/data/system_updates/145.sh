#!/bin/bash
set -o errexit

function IsSystemDSystem {
    return $(ps -p 1 -o comm=)=="systemd"
}

if [ ! IsSystemDSystem ]; then
    echo "No systemd found - skipping further installation"
    exit 0;
fi

cp --archive --verbose /usr/local/ub_tools/cpp/solr_restart_error_notification.sh /usr/local/bin

vufind_service_file=/etc/systemd/system/vufind.service


if [ ! -f ${vufind_service_file} ];
    echo "Could not find vufind.service file - skipping further installation"
    exit 0
fi

if grep --silent "Restart=on-failure" ${vufind_service_file}; then
    echo "Restart command seems already present - skipping installation"
    exit 0
fi

sed --in-place --regexp-extended --expression '/^ExecStart=/a\' \
    --expression 'Restart=on-failure\nExecStopPost=/usr/local/bin/solr_restart_error_notification.sh %N' \
    ${vufind_service_file}

systemctl daemon-reload
systemctl restart vufind
