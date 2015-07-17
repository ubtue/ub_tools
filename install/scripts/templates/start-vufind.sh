#
# WARNING: This file is auto generated. Changes might get lost.
#
# Please modify the template (github.com/ubtue/ub_tools/install/scripts/templates)
#

# paths
export JAVA_HOME="/usr/lib/jvm/default-java"
export VUFIND_HOME="{{{VUFIND_HOME}}}"
export VUFIND_LOCAL_DIR="{{{VUFIND_LOCAL_DIR}}}"

if [ command -v systemctl > /dev/null ] ; then
  systemctl stop vufind
  systemctl stop httpd
  systemctl stop mariadb
else
  /etc/init.d/mysql stop
  /etc/init.d/apache2 stop
  /etc/init.d/vufind stop
fi

# Wait to get everything right.
sleep 4

# link default location
sudo ln -sfT {{{CLONE_DIRECTORY_PATH}}} {{{VUFIND_HOME}}}

# start server
if [ command -v systemctl > /dev/null ] ; then
  systemctl start vufind
  systemctl start httpd
  systemctl start mariadb
else
  /etc/init.d/mysql start
  /etc/init.d/apache2 start
  /etc/init.d/vufind start
fi
