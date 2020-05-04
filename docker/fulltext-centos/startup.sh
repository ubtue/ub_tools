#!/bin/bash
# start mysqld_safe as last process, because it doesnt run in background
# if nothing runs in foreground, docker container will stop!
su  -c /usr/share/elasticsearch/bin/elasticsearch -s /bin/bash elasticsearch
