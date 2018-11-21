#!/bin/bash
/apps/squid/sbin/squid -z -f /apps/squid.conf.https_proxy
/apps/squid/sbin/squid -NsY -f /apps/squid.conf.https_proxy
