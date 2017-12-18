#!/bin/bash
#
# create zotero translation server docker file using our own forked translators repository
# - clone translation-server-repository
# - overwrite translators repository URL
# - build docker image
if [ -e /tmp/zts ]; then
    rm -r /tmp/zts
fi
git clone --recursive https://github.com/zotero/translation-server.git /tmp/zts
sed -i "s,https://github.com/zotero/translators,https://github.com/ubtue/zotero-translators.git," /tmp/zts/Dockerfile

docker build -t zts /tmp/zts
