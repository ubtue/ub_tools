#!/bin/bash
# Dump data needed by sync_qs_settings.sh
set -o errexit -o nounset


CONF="/usr/local/var/lib/tuelib/ub_tools.conf"
QS_TABLE="metadata_presence_tracer"
ZEDER_ID_TABLE="zeder_journals"
SECTION="Database"
DATABASE=$(inifile_lookup ${CONF} ${SECTION} sql_database)
USERNAME=$(inifile_lookup ${CONF} ${SECTION} sql_username)
PASSWORD=$(inifile_lookup ${CONF} ${SECTION} sql_password)

mysqldump --user="$USERNAME" --password="$PASSWORD" "$DATABASE" "$QS_TABLE" "$ZEDER_ID_TABLE" > qs_sync_$(hostname --short)_$(date +%y%m%d).sql
