#!/bin/bash
#
LATEST_CONTAINER_ID=$(docker ps -q --filter ancestor=zts)
if [ -z "$LATEST_CONTAINER_ID" ]; then
	echo "no running container detected"
else
	echo "stopping container $LATEST_CONTAINER_ID"
	docker stop $LATEST_CONTAINER_ID
fi
