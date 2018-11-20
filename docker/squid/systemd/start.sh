#!/bin/bash
#
LATEST_CONTAINER_ID=$(docker ps --all --latest --quiet --filter ancestor=squidproxy)
if [ -z "$LATEST_CONTAINER_ID" ]; then
    echo "run new container"
    docker run --publish 3128-3130:3128-3130 --network ub --name squidproxy --network-alias=squid  squidproxy
    LATEST_CONTAINER_ID=$(docker ps --all --latest --quiet --filter ancestor=squidproxy)
else
    echo "reuse existing container $LATEST_CONTAINER_ID"
    docker start $LATEST_CONTAINER_ID
fi

LATEST_CONTAINER_PID=$(docker inspect --format="{{.State.Pid}}" $LATEST_CONTAINER_ID)
echo $LATEST_CONTAINER_PID > /var/run/squid.pid
