#!/bin/bash
#
LATEST_CONTAINER_ID=$(docker ps --all --latest --quiet --filter ancestor=zts)
if [ -z "$LATEST_CONTAINER_ID" ]; then
    echo "run new container"

    DOCKER_CMD="docker run --publish 1969:1969 --network ub --name zts --network-alias=zts"
    if [ -n "${ZTS_PROXY}" ]; then
        DOCKER_CMD+=" --env HTTP_PROXY=$ZTS_PROXY --env HTTPS_PROXY=$ZTS_PROXY --env NODE_TLS_REJECT_UNAUTHORIZED=0"
    fi
    DOCKER_CMD+=" --detach zts"

    $DOCKER_CMD
    LATEST_CONTAINER_ID=$(docker ps --all --latest --quiet --filter ancestor=zts)
else
    echo "reuse existing container $LATEST_CONTAINER_ID"
    docker start $LATEST_CONTAINER_ID
fi

LATEST_CONTAINER_PID=$(docker inspect --format="{{.State.Pid}}" $LATEST_CONTAINER_ID)
echo $LATEST_CONTAINER_PID > /var/run/zts.pid
