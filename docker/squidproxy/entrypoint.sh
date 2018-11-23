#!/bin/bash

# create cache directories if necessary
# (only on first start, because we might want to stop/start container)
# this should be done only once in Dockerfile, but it didn't work!
if [ ! -d "/apps/squid/var/cache/squid/00" ]; then
    /apps/squid/sbin/squid -z -f /apps/squid.conf.https_proxy
fi

# start proxy service
/apps/squid/sbin/squid -NsY -f /apps/squid.conf.https_proxy
