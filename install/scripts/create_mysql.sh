#! /bin/bash
#
# This script drops the vufind-user and the vufind-database and creates both from scratch.
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

This script drops the vufind-user and the vufind-database and creates both from scratch.

USAGE: ${0##*/} MYSQL-ROOT-PASSWORD MYSQL-VUFIND-PASSWORD

MYSQL-ROOT-PASSWORD       The mysql root password
MYSQL-VUFIND-PASSWORD     The mysql vufind password

EOF
}

if [ "$#" -ne 2 ] && [ "$#" -ne 4 ] ; then
  echo "ERROR: Illegal number of parameters!"
  echo "Parameters: $*"
  show_help
  exit 1
fi

ROOT_PASSWORD="$1"
VUFIND_PASSWORD="$2"

# There is no 'DROP USER IF EXISTS' in MySQL. That's why we have to check it manually...
userExists=$(mysql -D "mysql" --user=root --password="$ROOT_PASSWORD" --execute="SELECT 1 FROM user WHERE User = 'vufind';")
if [ "$userExists" ] ; then
	mysql --user=root --password="$ROOT_PASSWORD" --execute="DROP USER 'vufind'@'localhost';"
fi

mysql --user=root --password="$ROOT_PASSWORD" --execute="DROP DATABASE IF EXISTS vufind;"
mysql --user=root --password="$ROOT_PASSWORD" < "$SCRIPT_DIR/create_mysql_database.sql"
mysql --user=root --password="$ROOT_PASSWORD" --execute="GRANT SELECT,INSERT,UPDATE,DELETE ON vufind.* TO 'vufind'@'localhost' IDENTIFIED BY '$VUFIND_PASSWORD' WITH GRANT OPTION; FLUSH PRIVILEGES;"
