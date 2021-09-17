#!/bin/bash
#
# script library for docker scripts
#


# \brief    Get latest existing container id by image, no matter if the container is running or not.
# \param    $1 The name of the docker image of the container
function get_latest_container_id {
    docker ps --all | grep -i "$1" | awk '{print $1}' | head --lines 1
}


# \brief    Get latest running container id by image
# \param    $1 The name of the docker image of the container
function get_latest_running_container_id {
    docker ps | grep -i "$1" | awk '{print $1}' | head --lines 1
}


# \brief    Get latest running container pid
# \param    $1 The name of the docker image of the container
function get_latest_running_container_pid {
    LATEST_CONTAINER_ID=$(get_latest_running_container_id $1)
    docker inspect --format="{{.State.Pid}}" $LATEST_CONTAINER_ID
}


# \brief    Get latest running container id by image
# \param    $1 The name of the docker image of the container
function get_pidfile {
    #Make sure container names like X/Y are appropriately mapped to a X_Y.pid scheme
    echo "/var/run/${1//\//_}.pid"
}


# \brief    stop & delete container if exists, delete image if exists, build image, start container
# \note     docker container needs to be installed as systemd unit for this to work!
# \param    $1 The name of the docker image and systemd unit
function rebuild_and_restart_service {
    # stop container if exists
    echo "stopping systemctl service $1"
    systemctl stop $1

    # delete container if exists
    LATEST_CONTAINER_ID=$(get_latest_container_id $1)
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


# \brief    reuse existing or start new container (and create pidfile)
# \param    $1 The name of the docker image of the container
# \param    $2 The "docker run" command to use if no container exists
function start_or_run_container {
    LATEST_CONTAINER_ID=$(get_latest_container_id $1)
    if [ -z "$LATEST_CONTAINER_ID" ]; then
        echo "run new container"
        $2
    else
        echo "reuse existing container $LATEST_CONTAINER_ID"
        docker start $LATEST_CONTAINER_ID
    fi

    LATEST_CONTAINER_PID=$(get_latest_running_container_pid $1)
    echo $LATEST_CONTAINER_PID > $(get_pidfile $1)
}


# \brief    stop container (and delete pidfile)
# \param    $1 The name of the docker image of the container
function stop_container {
    LATEST_RUNNING_CONTAINER_ID=$(get_latest_running_container_id $1)
    if [ -z "$LATEST_RUNNING_CONTAINER_ID" ]; then
        echo "no running container detected"
    else
        echo "stopping container $LATEST_RUNNING_CONTAINER_ID"
        docker stop $LATEST_RUNNING_CONTAINER_ID
    fi

    PIDFILE=$(get_pidfile $1)
    if [ -e $PIDFILE ]; then
        rm $PIDFILE
    fi
}
