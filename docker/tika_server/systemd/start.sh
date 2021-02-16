#!/bin/bash
LIB_FILE="$(dirname $(readlink --canonicalize "$0"))/../../lib.sh"
. $LIB_FILE

start_or_run_container "apache/tika" "docker run --publish 9998:9998 --detach apache/tika"
