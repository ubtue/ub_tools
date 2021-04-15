#!/bin/bash
#
DOCKER_ZTS_DIR=/usr/local/ub_tools/docker/zts
for keyfile in ${DOCKER_ZTS_DIR}/keys/*; do
    export $(cat ${keyfile})
done
rm -rf /tmp/zotero-translation-server
git clone --recurse-submodules https://github.com/ubtue/zotero-translation-server /tmp/zotero-translation-server
cp ${DOCKER_ZTS_DIR}/.dockerignore /tmp/zotero-translation-server
cp -r ${DOCKER_ZTS_DIR}/extra_certs /tmp/zotero-translation-server
cd /tmp/zotero-translation-server
cat Dockerfile | envsubst | docker build -t zts_dev . -f -
rm -rf /tmp/zotero-translation-server
