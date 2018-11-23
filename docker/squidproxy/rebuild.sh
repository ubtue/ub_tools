#!/bin/bash
#

# stop & delete container if exists
systemctl stop squidproxy
LATEST_CONTAINER_ID=$(docker ps --all --quiet --filter ancestor=squidproxy)
if [ -z "$LATEST_CONTAINER_ID" ]; then
    echo "no existing container detected"
else
    echo "deleting container $LATEST_CONTAINER_ID"
    docker rm $LATEST_CONTAINER_ID
fi

# delete image if exists
IMAGE_ID=$(docker images --quiet squidproxy)
if [ -z "$IMAGE_ID" ]; then
    echo "no existing image detected"
else
    echo "deleting image $IMAGE_ID"
    docker rmi $IMAGE_ID
fi

# build image
DIR="$(dirname $(readlink --canonicalize "$0"))"
$DIR/build.sh

# start service
systemctl start squidproxy
