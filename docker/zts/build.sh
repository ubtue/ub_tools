#!/bin/bash
#
rm -rf /tmp/zotero-translation-server
git clone --recurse-submodules https://github.com/ubtue/zotero-translation-server /tmp/zotero-translation-server
cp /usr/local/ub_tools/docker/zts/* /tmp/zotero-translation-server
cp /usr/local/ub_tools/docker/zts/.dockerignore /tmp/zotero-translation-server
cp -r /usr/local/ub_tools/docker/zts/extra_certs /tmp/zotero-translation-server
cd /tmp/zotero-translation-server
docker build -t zts .
rm -rf /tmp/zotero-translation-server
