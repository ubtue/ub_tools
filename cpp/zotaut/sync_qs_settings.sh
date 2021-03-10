#!/bin/bash
# Synchronize QS settings from another database
# c.f. dump_
set -o errexit -o nounset

if [ $# != 1 ]; then
    echo "usage $0 external_metadata_presence_tracer_and_zeder_journal.sql"
    exit 1
fi


FILE_TO_SYNC=$1
CONF="/usr/local/var/lib/tuelib/ub_tools.conf"
QS_TABLE="metadata_presence_tracer"
ZEDER_ID_TABLE="zeder_journals"
SECTION="Database"
DATABASE=$(inifile_lookup ${CONF} ${SECTION} sql_database)
TMP_DATABASE=${DATABASE}_tmp
USERNAME=$(inifile_lookup ${CONF} ${SECTION} sql_username)
PASSWORD=$(inifile_lookup ${CONF} ${SECTION} sql_password)
CURRENT_DATE=$(date +%y%m%d)

# Clean up 
mysql --user="${USERNAME}" --password="${PASSWORD}" --execute "DROP DATABASE IF EXISTS ${TMP_DATABASE}"

# Create a new temporary database to work on, create an id mapping an create a new table with correct original journal ids
mysql --user="${USERNAME}" --password="${PASSWORD}" --execute "CREATE DATABASE ${TMP_DATABASE}"
mysql --user="${USERNAME}" --password="${PASSWORD}" "${TMP_DATABASE}" < $FILE_TO_SYNC
mysql --user="${USERNAME}" --password="${PASSWORD}" "${TMP_DATABASE}" --execute  "CREATE TABLE ${TMP_DATABASE}.id_mapping SELECT ${DATABASE}.zeder_journals.id AS orig, ${TMP_DATABASE}.zeder_journals.id AS new FROM ${TMP_DATABASE}.zeder_journals JOIN ${DATABASE}.zeder_journals ON ${TMP_DATABASE}.zeder_journals.zeder_instance=${DATABASE}.zeder_journals.zeder_instance AND ${TMP_DATABASE}.zeder_journals.zeder_id=${DATABASE}.zeder_journals.zeder_id COLLATE utf8mb4_general_ci"
mysql --user="${USERNAME}" --password="${PASSWORD}" "${TMP_DATABASE}" --execute "CREATE TABLE ${TMP_DATABASE}.metadata_presence_tracer_new SELECT ${TMP_DATABASE}.id_mapping.orig AS journal_id, marc_field_tag, marc_subfield_code, regex, record_type, field_presence FROM id_mapping RIGHT JOIN metadata_presence_tracer ON id_mapping.new=metadata_presence_tracer.journal_id"

# Backup

mysql --user="${USERNAME}" --password="${PASSWORD}" "${TMP_DATABASE}" --execute "DROP TABLE IF EXISTS ${DATABASE}.metadata_presence_tracer_${CURRENT_DATE}"
mysql --user="${USERNAME}" --password="${PASSWORD}" "${TMP_DATABASE}" --execute "CREATE TABLE ${DATABASE}.metadata_presence_tracer_${CURRENT_DATE} SELECT * FROM metadata_presence_tracer"

#Replace the original
mysql --user="${USERNAME}" --password="${PASSWORD}" "${TMP_DATABASE}" --execute "DROP TABLE ${DATABASE}.metadata_presence_tracer"
mysql --user="${USERNAME}" --password="${PASSWORD}" "${TMP_DATABASE}" --execute "CREATE TABLE ${DATABASE}.metadata_presence_tracer SELECT * FROM ${TMP_DATABASE}.metadata_presence_tracer"

#Clean up
mysql --user="${USERNAME}" --password="${PASSWORD}" "${TMP_DATABASE}" --execute "DROP DATABASE ${TMP_DATABASE}"
