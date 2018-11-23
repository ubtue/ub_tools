#!/bin/bash
#
LIB_FILE="$(dirname $(readlink --canonicalize "$0"))/../../lib.sh"
. $LIB_FILE

DOCKER_CMD="docker run --publish 3128-3130:3128-3130 --detach squidproxy"
start_or_run_container squidproxy "$DOCKER_CMD"
