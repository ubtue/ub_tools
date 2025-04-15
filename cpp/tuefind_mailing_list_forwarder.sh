#!/bin/bash
set -o errexit -o nounset

tmpdir=$(mktemp --directory -t tuefind_mailing_list_forwarder_XXXXXX)
trap "find ${tmpdir} -mindepth 1 -delete && rmdir ${tmpdir}" EXIT


function ExtractTueFindDBCredentials() {
    source /etc/profile.d/vufind.sh
    uri=$(/usr/local/bin/inifile_lookup ${VUFIND_LOCAL_DIR}/config/vufind/local_overrides/database.conf "" database)
    user=$(echo $uri | sed -E 's|^mysql://([^:]+):.*|\1|')
    password=$(echo $uri | sed -E 's|^mysql://[^:]+:([^@]+)@.*|\1|')
    host=$(echo $uri | sed -E 's|^mysql://[^@]+@([^/:]+)[/:].*|\1|')
    port=$(echo $uri | sed -E 's|^mysql://[^:]+:[^@]+@[^:/]+:?([^/]*)/.*|\1|')
    database=$(echo $uri | sed -E 's|^mysql://[^/]+/([^?]+).*|\1|')
}


function TemporaryMySQLConfig() {
   echo "[Client]"
   echo "user = $user"
   echo "password = $password"
   echo "host = $host"
}


function GetDelay() {
   MINWAIT=0.5
   MAXWAIT=3
   echo "$MINWAIT + $RANDOM * ($MAXWAIT - $MINWAIT) / 32767" | bc
}


cat > ${tmpdir}/mail.eml

ExtractTueFindDBCredentials
for email in $(mysql --defaults-extra-file=<(TemporaryMySQLConfig)  \
               --skip-column-names --batch \
               -e 'SELECT email FROM user WHERE krimdok_subscribed_to_newsletter=true' \
               $database); 
do
    sleep $(GetDelay)
    /sbin/sendmail $email < ${tmpdir}/mail.eml
done
