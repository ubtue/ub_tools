#!/bin/bash
#
LATEST_RUNNING_CONTAINER_ID=$(docker ps --latest --quiet --filter ancestor=squidproxy)
if [ -z "$LATEST_RUNNING_CONTAINER_ID" ]; then
    echo "no running container detected"
else
    echo "stopping container $LATEST_RUNNING_CONTAINER_ID"
    docker stop $LATEST_RUNNING_CONTAINER_ID
fi

if [ -e /var/run/squid.pid ]; then
    rm /var/run/squid.pid
fi
