#!/bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Creates a .htaccess with a .htpasswd configuration to protect the access
# to the website.
#

make -C "$VUFIND_HOME/../ub_tools" root_install
