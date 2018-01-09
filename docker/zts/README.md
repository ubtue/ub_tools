# Custom zotero translation server docker container with ubtue translators

## Basics
* use build.sh to build docker image
* use zts.service for systemd

## Directory structure inside docker container:
* translation-server: /opt/translation-server
* translators directory: /opt/translation-server/app/translators
  * Note: You can use "git pull" to pull current translators from ubtue!

