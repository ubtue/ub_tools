#!/bin/bash
#
# create zotero translation server docker file using our own forked translators repository
# - clone translation-server-repository
# - overwrite translators repository URL
# - build docker image
rm -rf /tmp/zts
git clone --recursive https://github.com/zotero/translation-server.git /tmp/zts
sed --in-place "s,https://github.com/zotero/translators,https://github.com/ubtue/zotero-translators.git," /tmp/zts/Dockerfile

docker build --tag=zts /tmp/zts
