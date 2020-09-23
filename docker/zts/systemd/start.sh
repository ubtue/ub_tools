#!/bin/bash
#
LIB_FILE="$(dirname $(readlink --canonicalize "$0"))/../../lib.sh"
. $LIB_FILE

# We use journald+syslog as log driver to redirect output to a single known file
# which can later be made accessible via zotero_cgi.
#
# NOTE:
# - When using journald, `docker logs` will not work anymore.
# - Adding the apache user to docker group & calling `docker logs` will not work under CentOS 8,
#   see https://www.projectatomic.io/blog/2015/08/why-we-dont-let-non-root-users-run-docker-in-centos-fedora-or-rhel/
# - Also tried syslog, but that is not compatible with podman-docker under CentOS 8,
#   so we use journald which redirects to syslog per default.
# - We use --log-opt tag=zts for Ubuntu, which also doesn't work under CentOS 8 with podman-docker
#   (bug has been fixed 2020, new version not available on CentOS 8 yet, default will be "conmon").
DOCKER_CMD="docker run --publish 1969:1969 --log-driver journald --log-opt tag=zts"
if [ -n "${ZTS_PROXY}" ]; then
    DOCKER_CMD+=" --env HTTP_PROXY=$ZTS_PROXY --env HTTPS_PROXY=$ZTS_PROXY --env NODE_TLS_REJECT_UNAUTHORIZED=0"
fi
DOCKER_CMD+=" --detach zts"

start_or_run_container zts "$DOCKER_CMD"
