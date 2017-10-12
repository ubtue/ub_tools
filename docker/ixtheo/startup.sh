#!/bin/bash
# start mysqld_safe as last process, because it doesnt run in background
# if nothing runs in foreground, docker container will stop!
sudo -u solr /usr/local/vufind/solr.sh start
apachectl
mysqld_safe
