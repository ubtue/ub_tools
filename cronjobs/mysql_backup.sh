#!/bin/bash
set -o errexit
# TARGET: Backup-Ziel
# IGNORE: Liste zu ignorierender Datenbanken (durch | getrennt)
# CONF: MySQL Config-Datei, welche die Zugangsdaten enthaelt
TARGET=/var/lib/tuelib/backups/mysql
IGNORE="phpmyadmin|mysql|information_schema|performance_schema|test"
CONF=/etc/mysql/debian.cnf
if [ ! -r $CONF ]; then echo "$0 - auf $CONF konnte nicht zugegriffen werden"; exit 1; fi
if [ ! -d $TARGET ] || [ ! -w $TARGET ]; then echo "$0 - Backup-Verzeichnis nicht beschreibbar"; exit 1; fi

DBS="$(/usr/bin/mysql --defaults-extra-file=$CONF -Bse 'show databases' | /bin/grep -Ev $IGNORE)"
NOW=$(date +"%Y-%m-%d")

for DB in $DBS; do
    /usr/bin/mysqldump --defaults-extra-file=$CONF --skip-extended-insert --skip-comments $DB > $TARGET/$DB.sql
done

if [ -x /usr/bin/git ] && [ -d ${TARGET}/.git/ ]; then
  cd $TARGET
  /usr/bin/git add .
  /usr/bin/git commit -m "$NOW"
else
  echo "$0 - git nicht verfuegbar oder Backup-Ziel nicht unter Versionskontrolle"
  exit 2
fi

echo "$0 - Backup von $NOW erfolgreich durchgefuehrt"
exit 0


