#!/bin/bash
#
# This script is meant to be placed in /etc/profile.d/
# The env file will be used for bash as well as zts docker container, if exists
ENV_FILE=/usr/local/etc/zts_proxy.env
if [ -f $ENV_FILE ]; then
	for LINE in $(cat $ENV_FILE); do
		CMD="export "
		CMD+=$LINE
		$CMD
	done
fi
