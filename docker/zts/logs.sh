#!/bin/bash
#
LIB_FILE="$(dirname $(readlink --canonicalize "$0"))/../lib.sh"
. $LIB_FILE

CONTAINER_ID="$(get_latest_container_id zts)"
docker logs $CONTAINER_ID
