#!/bin/bash
#
LIB_FILE="$(dirname $(readlink --canonicalize "$0"))/../../lib.sh"
. $LIB_FILE

# We use syslog as log driver because it's the easiest way to redirect output to a single known file
# which can later be made accessible via zotero_cgi.
# (Adding the apache user to docker group & calling `docker logs` will not work under CentOS 8).
# (see https://www.projectatomic.io/blog/2015/08/why-we-dont-let-non-root-users-run-docker-in-centos-fedora-or-rhel/)
DOCKER_CMD="docker run --publish 1969:1969 --log-driver syslog --log-opt tag=zts"
if [ -n "${ZTS_PROXY}" ]; then
    DOCKER_CMD+=" --env HTTP_PROXY=$ZTS_PROXY --env HTTPS_PROXY=$ZTS_PROXY --env NODE_TLS_REJECT_UNAUTHORIZED=0"
fi
DOCKER_CMD+=" --detach zts"

start_or_run_container zts "$DOCKER_CMD"
