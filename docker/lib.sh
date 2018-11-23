#!/bin/bash
#
# script library for docker scripts
#

# \brief    stop & delete container if exists, delete image if exists, build image, start container
# \note     docker container needs to be installed as systemd unit for this to work!
# \param    $1 The name of the docker image and systemd unit
function rebuild_and_restart_service {
    # stop & delete container if exists
    systemctl stop $1
    LATEST_CONTAINER_ID=$(docker ps --all --quiet --filter ancestor=$1)
    if [ -z "$LATEST_CONTAINER_ID" ]; then
        echo "no existing container detected"
    else
        echo "deleting container $LATEST_CONTAINER_ID"
        docker rm $LATEST_CONTAINER_ID
    fi

    # delete image if exists
    IMAGE_ID=$(docker images --quiet $1)
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
    systemctl start $1
}
