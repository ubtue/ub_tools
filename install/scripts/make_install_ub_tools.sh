#!/bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Creates a .htaccess with a .htpasswd configuration to protect the access
# to the website.
#

# This is needed to let jetty extract the libraries, which are needed by
# our solr plugins and co.

make -C "$VUFIND_HOME/../ub_tools" install
