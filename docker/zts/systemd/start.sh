#!/bin/bash
#
LATEST_CONTAINER_ID=$(docker ps --all --latest --quiet --filter ancestor=zts)
if [ -z "$LATEST_CONTAINER_ID" ]; then
    echo "run new container"
    docker run -p 1969:1969 -d zts
    LATEST_CONTAINER_ID=$(docker ps --all --latest --quiet --filter ancestor=zts)
else
    echo "reuse existing container $LATEST_CONTAINER_ID"
    docker start $LATEST_CONTAINER_ID
fi

LATEST_CONTAINER_PID=$(docker inspect --format="{{.State.Pid}}" $LATEST_CONTAINER_ID)
echo $LATEST_CONTAINER_PID > /var/run/zts.pid
