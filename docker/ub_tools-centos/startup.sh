#!/bin/bash
# start mysqld_safe as last process, because it doesnt run in background
# if nothing runs in foreground, docker container will stop!
apachectl
mysqld_safe
