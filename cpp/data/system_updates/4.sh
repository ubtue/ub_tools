#!/bin/bash
CONFIG_FILE=/etc/logrotate.d/mariadb
if [[ -f /etc/redhat-release ]] && [[ -f ${CONFIG_FILE} ]]; then
    sed  --in-place '/\/var\/log\/mariadb\/mariadb.log {/a \ \ \ \ \ \ \ \ delaycompress' ${CONFIG_FILE}
fi
