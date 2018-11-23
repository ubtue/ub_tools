#!/bin/bash
if [ ! -d "/apps/squid/var/cache/squid/00" ]; then
    /apps/squid/sbin/squid -z -f /apps/squid.conf.https_proxy
fi
/apps/squid/sbin/squid -NsY -f /apps/squid.conf.https_proxy
