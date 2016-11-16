#! /bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Writes private vufind configs, like mysql passwords.
#
set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

show_help() {
	cat << EOF
Writes private vufind configs, like mysql passwords.

USAGE: ${0##*/} SERVER_URL NAME EMAIL MYSQL_VUFIND_PASSWORD

SERVER_URL              The public url of this server
NAME                    The name of the subdirectory of the cloned repository
EMAIL                   The support email adress of this server
MYSQL_VUFIND_PASSWORD   The mysql vufind password
EOF
}


if [ "$#" -ne 4 ]; then
  echo "ERROR: Illegal number of parameters!"
  echo "Parameters: $*"
  show_help
  exit 1
fi

SERVER_URL="$1"
NAME="$2"
EMAIL="$3"
VUFIND_PASSWORD="$4"
LOCAL_OVERRIDES="$VUFIND_LOCAL_DIR/local_overrides"
SITE_CONFIG="$LOCAL_OVERRIDES/site.conf"
DATABASE_CONFIG="$LOCAL_OVERRIDES/database.conf"
DATABASE_SOLRMARC_CONFIG="$LOCAL_OVERRIDES/database_solrmarc.conf"
HTTP_CONFIG="$LOCAL_OVERRIDES/http.conf"

if [ ! -d  "$LOCAL_OVERRIDES" ] ; then
	mkdir "$LOCAL_OVERRIDES"
fi

# Site configs
sh -c "> $SITE_CONFIG"
echo "url   = \"https://$SERVER_URL\"" >> "$SITE_CONFIG"
echo "email = \"$EMAIL\""         >> "$SITE_CONFIG"
echo "title = \"$NAME\""         >> "$SITE_CONFIG"

# Database configs
sh -c "> $DATABASE_CONFIG"
echo "database = \"mysql://vufind:$VUFIND_PASSWORD@localhost/vufind\"" >> "$DATABASE_CONFIG"

# Database config for SolrMarc
sh -c "> $DATABASE_SOLRMARC_CONFIG"
echo "[Database]" >> "$DATABASE_SOLRMARC_CONFIG" >> "$DATABASE_SOLRMARC_CONFIG"
echo "database = \"mysql://vufind:$VUFIND_PASSWORD@localhost/vufind\"" >> "$DATABASE_SOLRMARC_CONFIG"


# Http configs
if [[ -r /etc/pki/tls/cert.pem ]] ; then
	echo "sslcafile = \"/etc/pki/tls/cert.pem\"" > "$HTTP_CONFIG"
else
	echo "sslcapath = \"/etc/ssl/certs\"" > "$HTTP_CONFIG"
fi
