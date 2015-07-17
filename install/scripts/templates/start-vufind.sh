#
# WARNING: This file is auto generated. Changes might get lost.
#
# Please modify the template (github.com/ubtue/ub_tools/install/scripts/templates)
#

# paths
export JAVA_HOME="/usr/lib/jvm/default-java"
export VUFIND_HOME="{{{VUFIND_HOME}}}"
export VUFIND_LOCAL_DIR="{{{VUFIND_LOCAL_DIR}}}"

if [ -x "$(command -v systemctl)" ] ; then
  sudo systemctl stop vufind
  sudo systemctl stop httpd
  sudo systemctl stop mariadb
else
  sudo /etc/init.d/mysql stop
  sudo /etc/init.d/apache2 stop
  sudo /etc/init.d/vufind stop
fi

# Wait to get everything right.
sleep 4

# link default location
sudo ln -sfT {{{CLONE_DIRECTORY_PATH}}} {{{VUFIND_HOME}}}

# start server
if [ -x "$(command -v systemctl)" ] ; then
  sudo systemctl start vufind
  sudo systemctl start httpd
  sudo systemctl start mariadb
else
  sudo /etc/init.d/mysql start
  sudo /etc/init.d/apache2 start
  sudo /etc/init.d/vufind start
fi
