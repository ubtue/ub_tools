#!/bin/bash
#
rm -rf /tmp/zotero-translation-server-v2
git clone --recurse-submodules git@github.com:zotero/translation-server-v2.git /tmp/zotero-translation-server-v2
cp /usr/local/ub_tools/docker/zts2/* /tmp/zotero-translation-server-v2
cp /usr/local/ub_tools/docker/zts2/.dockerignore /tmp/zotero-translation-server-v2
cd /tmp/zotero-translation-server-v2
docker build -t zts2 .

