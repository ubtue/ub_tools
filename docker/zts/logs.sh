#!/bin/bash
#
LIB_FILE="$(dirname $(readlink --canonicalize "$0"))/../lib.sh"
. $LIB_FILE

CONTAINER_ID="$(get_latest_container_id zts)"

if [ "$#" -ne "0" ]; then
    docker logs --since $1 $CONTAINER_ID
else
    docker logs $CONTAINER_ID
fi
