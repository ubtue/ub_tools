#!/bin/bash
#
LIB_FILE="$(dirname $(readlink --canonicalize "$0"))/../lib.sh"
. $LIB_FILE

rebuild_and_restart_service tika_server
