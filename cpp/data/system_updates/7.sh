#!/bin/bash
set -o errexit -o nounset

readonly SYSTEMD_SERVICE_DIR=/etc/systemd/system/
install --owner=root --group=root --mode=644 \
        /usr/local/ub_tools/cpp/data/installer/boot_notification.service \
        $(SYSTEMD_SERVICE_DIR)
/usr/bin/systemctl start boot_notification.service
