#!/bin/bash

# This script cleans the systemd-private-XXX-apache-YYY/tmp/vufind_sessions direction from all session older than three days and not a logged in session (i.e. session_file size < 1K)

if [[ $# != 1 ]]; then
    echo "usage: $0 notification_email_address"
    exit 1
fi

#cf. https://serverfault.com/questions/786211/access-files-in-system-tmp-directory-when-using-privatetmp
apache_pid=$(systemctl show --property=MainPID --value apache2.service)
nsenter -t $apache_pid -m find /tmp/vufind_sessions/ -mindepth 1 -maxdepth 1 -mtime +3 -size -1000 -delete

if [ $? -ne 0 ]; then
     mutt -H - <<END_OF_EMAIL
To: $1
Subject: $0 Failed
X-Priority: 1

Cleanup of session files failed...
END_OF_EMAIL

fi

