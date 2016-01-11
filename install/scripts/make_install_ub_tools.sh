#!/bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Creates a .htaccess with a .htpasswd configuration to protect the access
# to the website.
#

# This is needed to let jetty extract the libraries, which are needed by
# our solr plugins and co.


WAR_FILE="$VUFIND_HOME/solr/jetty/webapps/solr.war"
WAR_TARGET="$VUFIND_HOME/solr/jetty/work/jetty-0.0.0.0-8080-solr.war-_solr-any-/webapp/"

mkdir --parent "$WAR_TARGET"
unzip "$WAR_FILE" -d "$WAR_TARGET"

make -C "$VUFIND_HOME/../ub_tools" root_install
