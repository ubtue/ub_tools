#!/bin/bash
#
LATEST_CONTAINER_ID=$(docker ps --all --latest --quiet --filter ancestor=squid)
if [ -z "$LATEST_CONTAINER_ID" ]; then
    echo "run new container"
    docker run --publish 3128-3129:3128-3129 --network ub --name squid --network-alias=squid \
               --volume /usr/local/ub_tools/docker/squid/squid.conf:/etc/squid/squid.conf --detach squid
    LATEST_CONTAINER_ID=$(docker ps --all --latest --quiet --filter ancestor=squid)
else
    echo "reuse existing container $LATEST_CONTAINER_ID"
    docker start $LATEST_CONTAINER_ID
fi

LATEST_CONTAINER_PID=$(docker inspect --format="{{.State.Pid}}" $LATEST_CONTAINER_ID)
echo $LATEST_CONTAINER_PID > /var/run/squid.pid
