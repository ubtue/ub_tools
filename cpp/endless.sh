#!/bin/bash

trap 'echo Exiting; exit -1' SIGTERM

while :; do
    sleep 1
    echo Looping
done
