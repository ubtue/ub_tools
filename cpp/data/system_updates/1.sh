#!/bin/bash
install --owner=root --group=root --mode=644 /usr/local/ub_tools/cpp/data/installer/syslog.ub_tools.conf /etc/rsyslog.d/40-ub_tools.conf
systemctl restart rsyslog
