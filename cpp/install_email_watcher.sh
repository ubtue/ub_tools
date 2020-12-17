#!/bin/bash
set -o errexit -o nounset -o pipefail

# Make sure we're root:
if [[ $(id --user) != 0 ]]; then
    echo 'This script must be executed as root!'
    exit 1
fi

# Create user email_watcher unless this user already exists:
if [[ $(getent passwd email_watcher) == "" ]]; then
    useradd --create-home email_watcher
fi

# Install the config file for fetchmail for user email_watcher:
install --owner=email_watcher --group=email_watcher --mode=0600 \
        /mnt/ZE020150/FID-Entwicklung/ub_tools/email_watcher.fetchmailrc \
        ~email_watcher/.fetchmailrc

# Set up a crontab for email_watcher:
echo '*/10 * * * * /usr/local/bin/email_watcher.sh' > /tmp/email_watcher.crontab
crontab -u email_watcher /tmp/email_watcher.crontab
rm /tmp/email_watcher.crontab
