#!/bin/bash
# We explicitly do NOT use -o nounset here, because $TUEFIND_FLAVOUR will not exist
# on systems where only ub_tools is installed, e.g. for zotaut.
set -o errexit -o pipefail
if [[ $TUEFIND_FLAVOUR == "ixtheo" ]]; then
    if [ -e /var/spool/cron/root ]; then # CentOS
        root_crontab=/var/spool/cron/root
    elif [ -e /var/spool/cron/crontabs/root ]; then # Ubuntu
        root_crontab=/var/spool/cron/crontabs/root
    else
        exit 0 # There is no crontab for root.
    fi
    sed --in-place '/^# END VUFIND AUTOGENERATED/i 30 22 * * 0 "$BIN/generate_kalliope_originators.py" > "${LOG_DIR}/generate_kalliope_originators.log" 2>&1' ${root_crontab}
    cp /usr/local/ub_tools/cronjobs/*_kalliope_* /usr/local/bin
    generate_kalliope_originators.py > /usr/local/var/log/tuefind/generate_kalliope_originators.log 2>&1
fi
