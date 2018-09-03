#!/bin/bash
#
rm -rf /tmp/zotero-translation-server
git clone --recurse-submodules https://github.com/ubtue/zotero-translation-server /tmp/zotero-translation-server
cp /usr/local/ub_tools/docker/zts2/* /tmp/zotero-translation-server
cp /usr/local/ub_tools/docker/zts2/.dockerignore /tmp/zotero-translation-server
cd /tmp/zotero-translation-server
docker build -t zts2 .

