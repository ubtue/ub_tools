#!/bin/bash
set -o errexit

function IsSystemDSystem {
    return $(ps -p 1 -o comm=)=="systemd"
}

if [ ! IsSystemDSystem ]; then
    echo "No systemd found - skipping further installation"
    exit 0;
fi

cp --archive --verbose /usr/local/ub_tools/cpp/data/installer/error_notification.service /usr/local/lib/systemd/system/
cp --archive --verbose /usr/local/ub_tools/cpp/error_notification.sh /usr/local/bin
systemctl enable error_notification

vufind_service_file=/usr/local/lib/systemd/system/vufind.service


if grep --silent "Restart=on-failure" ${vufind_service_file}; then
    echo "Restart command seems already present - skipping installation"
    exit 0
fi

sed --in-place --regexp-extended --expression '/^ExecStart=/a\' \
    --expression 'Restart=on-failure\nOnFailure=error_notification.service' \
    ${vufind_service_file}

systemctl daemon-reload

